#pragma once

#include <stdexcept>
#include <string>
#include <memory>
#include <vector>
#include "make_unique.hpp"
#include "prog.hpp"
#include "config.hpp"

namespace tmr {

	static std::unique_ptr<Atomic> mk_mega_malloc(std::string vname) {
		std::vector<std::unique_ptr<Statement>> stmts;
		stmts.push_back(std::make_unique<Malloc>(Var(vname)));
		stmts.push_back(SetNull(Next(vname)));
		stmts.push_back(std::make_unique<ReadInputAssignment>(Data(vname)));
		return std::make_unique<Atomic>(std::make_unique<Sequence>(std::move(stmts)));
	}

	static std::unique_ptr<Program> micheal_scott_queue(bool mega_malloc, bool age_compare, bool use_age_fields) {
		if (age_compare && !use_age_fields) throw std::logic_error("ITE may only compare age fields if they are used.");
		/* Program */

		// malloc(Head);
		// Head.next = NULL;
		// Tail = Head;
		std::unique_ptr<Sequence> init = Sqz(
			Mllc("Head"),
			SetNull(Next("Head")),
			Assign(Var("Tail"), Var("Head"))
		);


		// malloc(h);
		// h.next = NULL;
		// h.data = __in__;
		//
		// while (true) {
		//     t = Tail;
		//     n = t.next;
		//
		//     if (t == Tail) {
		//          if (n == NULL) {
		//               if (CAS(t.next, n, h)) *** enq(__in__) *** {
		//                    break;
		//               }
		//          } else {
		//               CAS(Tail, t, n);
		//          }
		//     }
		// }
		// CAS(Tail, t, h);
		std::unique_ptr<Statement> enqloop = Loop(Sqz(
			Assign(Var("t"), Var("Tail")),
			Assign(Var("n"), Next("t")),
			IfThen(
				EqCond(Var("t"), Var("Tail"), age_compare),
				Sqz(IfThenElse(
					EqCond(Var("n"), Null()),
					Sqz(
						IfThen(
							// CasCond(CAS(Next("t"), Null(), Var("h"), LinP(), use_age_fields)),
							CasCond(CAS(Next("t"), Var("n"), Var("h"), LinP(), use_age_fields)),
							Sqz(Brk())
					)),
					Sqz(CAS(Var("Tail"), Var("t"), Var("n"), use_age_fields))
				))
			),
			Kill("t"),
			Kill("n"),
			SetNull(Next("h"))
		));
		std::unique_ptr<Sequence> enqbody;
		if (mega_malloc) {
			enqbody = Sqz(
				mk_mega_malloc("h"),
				std::move(enqloop),
				CAS(Var("Tail"), Var("t"), Var("h"), use_age_fields)
			);
		} else {
			enqbody = Sqz(
				Mllc("h"),
				SetNull(Next("h")),
				Read("h"),
				std::move(enqloop),
				CAS(Var("Tail"), Var("t"), Var("h"), use_age_fields)
			);
		}
		assert(enqbody);

		// while (true) {
		//     h = Head;
		//     t = Tail;
		//     n = h.next *** [n == NULL] deq(empty) ***;
		//
		//     if (h == Head) {
		//          if (h == t) {
		//               if (n == NULL) {
		//                    __out__ = empty;
		//                    break;
		//               }
		//               CAS(Tail, t, n);
		//          } else {
		//               __out__ = n.data;
		//               if (CAS(Head, h, n)) *** deq(n.data) *** {
		//                    free(h);
		//                    break;
		//               }
		//          }
		//     }
		// }
		std::unique_ptr<Condition> linpc = CompCond(OCond(), CompCond(EqCond(Var("h"), Var("Head"), age_compare), CompCond(EqCond(Var("h"), Var("t")), EqCond(Var("n"), Null()))));
		std::unique_ptr<Sequence> deqbody = Sqz(Loop(Sqz(
			Assign(Var("h"), Var("Head")),
			Assign(Var("t"), Var("Tail")),
			Orcl(),
			Assign(Var("n"), Next("h"), LinP(std::move(linpc))),
			IfThenElse(
				EqCond(Var("h"), Var("Head"), age_compare),
				Sqz(
					ChkP(true),
					IfThenElse(
						EqCond(Var("h"), Var("t")),
						Sqz(
							IfThen(
								EqCond(Var("n"), Null()),
								Sqz(Brk())
							),
							CAS(Var("Tail"), Var("t"), Var("n"), use_age_fields)
						),
						Sqz(
							Write("n"),
							IfThen(
								CasCond(CAS(Var("Head"), Var("h"), Var("n"), LinP("n"), use_age_fields)),
								Sqz(
									Brk()
								)
							)
						)
					)
				),
				Sqz(ChkP(false))
			),
			Kill("h"),
			Kill("t"),
			Kill("n"),
			Kill()
		)));

		#if REPLACE_INTERFERENCE_WITH_SUMMARY
			// atomic {
			// 	t = Tail.next;
			// 	if (t == NULL) {
			// 		aux = malloc;
			// 		aux.data = __in__;
			// 		aux.next = NULL;
			// 		Tail.next = aux *** enq(__in__) ***;
			// 	} else {
			// 		Tail = t;
			// 	}
			// }
			auto enqsum = AtomicSqz(
				Assign(Var("t"), Next("Tail")),
				IfThenElse(
					EqCond(Var("t"), Null()),
					Sqz(
						Mllc("n"),
						SetNull(Next("n")),
						Read("n"),
						Assign(Next("Tail"), Var("n"), LinP())
					),
					Sqz(
						Assign(Var("Tail"), Var("t"))
					)
				)
			);

			// atomic {
			// 	n = Head.next;
			// 	if (Head == Tail) {
			// 		if (n == NULL) {
			// 			*** deq(empty) ***
			// 		} else {
			// 			Tail = n;
			// 		}
			// 	} else {
			//		Head = n *** deq(n.data) ***
			// 	}
			// }
			auto deqsum = AtomicSqz(
				Assign(Var("n"), Next("Head")),
				IfThenElse(
					EqCond(Var("Head"), Var("Tail")),
					Sqz(
						IfThenElse(
							EqCond(Var("n"), Null()),
							Sqz(LinP()),
							Sqz(Assign(Var("Tail"), Var("n")))
						)
					),
					Sqz(
						Assign(Var("Head"), Var("n"), LinP("n"))
					)
				)
			);
		#endif


		std::string name = "MichealScottQueue";

		return Prog(
			name,
			{"Head", "Tail"},
			{"h", "t", "n"},
			std::move(init),
			#if REPLACE_INTERFERENCE_WITH_SUMMARY
				Fun("enq", true, std::move(enqbody), std::move(enqsum)),
				Fun("deq", false, std::move(deqbody), std::move(deqsum))
			#else
				Fun("enq", true, std::move(enqbody)),
				Fun("deq", false, std::move(deqbody))
			#endif
		);
	}

	static std::unique_ptr<Program> dglm_queue(bool mega_malloc, bool age_compare, bool use_age_fields) {
		if (age_compare && !use_age_fields) throw std::logic_error("ITE may only compare age fields if they are used.");
		/* Program */

		// malloc(Head);
		// Head.next = NULL;
		// Tail = Head;
		std::unique_ptr<Sequence> init = Sqz(
			Mllc("Head"),
			SetNull(Next("Head")),
			Assign(Var("Tail"), Var("Head"))
		);


		// malloc(h);
		// h.next = NULL;
		// h.data = __in__;
		//
		// while (true) {
		//     t = Tail;
		//     n = t.next;
		//
		//     if (t == Tail) {
		//          if (n == NULL) {
		//               if (CAS(t.next, n, h)) *** enq(__in__) *** {
		//                    break;
		//               }
		//          } else {
		//               CAS(Tail, t, n);
		//          }
		//     }
		// }
		// CAS(Tail, t, h);
		std::unique_ptr<Statement> enqloop = Loop(Sqz(
			Assign(Var("t"), Var("Tail")),
			Assign(Var("n"), Next("t")),
			IfThen(
				EqCond(Var("t"), Var("Tail"), age_compare),
				Sqz(IfThenElse(
					EqCond(Var("n"), Null()),
					Sqz(
						IfThen(
							// CasCond(CAS(Next("t"), Null(), Var("h"), LinP(), use_age_fields)),
							CasCond(CAS(Next("t"), Var("n"), Var("h"), LinP(), use_age_fields)),
							Sqz(Brk())
					)),
					Sqz(CAS(Var("Tail"), Var("t"), Var("n"), use_age_fields))
				))
			),
			Kill("t"),
			Kill("n"),
			SetNull(Next("h"))
		));
		std::unique_ptr<Sequence> enqbody;
		if (mega_malloc) {
			enqbody = Sqz(
				mk_mega_malloc("h"),
				std::move(enqloop),
				CAS(Var("Tail"), Var("t"), Var("h"), use_age_fields)
			);
		} else {
			enqbody = Sqz(
				Mllc("h"),
				SetNull(Next("h")),
				Read("h"),
				std::move(enqloop),
				CAS(Var("Tail"), Var("t"), Var("h"), use_age_fields)
			);
		}
		assert(enqbody);

		// while (true) {
		//     h = Head;
		//     n = h.next *** [n == NULL] deq(empty) ***;

		//     if (h == Head) {
		//          if (n == NULL) {
		//               __out__ = empty;
		//               break;
		//          } else {
		//               __out__ = n.data;
		//               if (CAS(Head, h, n)) *** deq(n.data) *** {
		//                    free(h);
		//                    t = Tail;
		//                    if (h == t) {
		//                          CAS(Tail, t, n);
		//                    }
		//                    break;
		//               }
		//          }
		//     }
		// }
		std::unique_ptr<Condition> linpc = CompCond(OCond(), CompCond(EqCond(Var("h"), Var("Head"), age_compare), EqCond(Var("n"), Null())));
		std::unique_ptr<Sequence> deqbody = Sqz(Loop(Sqz(
			Assign(Var("h"), Var("Head")),
			Orcl(),
			Assign(Var("n"), Next("h"), LinP(std::move(linpc))),
			IfThenElse(
				EqCond(Var("h"), Var("Head"), age_compare),
				Sqz(
					ChkP(true),
					IfThenElse(
						EqCond(Var("n"), Null()),
						Sqz(Brk()),
						Sqz(
							Write("n"),
							IfThen(
								CasCond(CAS(Var("Head"), Var("h"), Var("n"), LinP("n"), use_age_fields)),
								Sqz(
									Assign(Var("t"), Var("Tail")),
									IfThen(
										EqCond(Var("h"), Var("t")),
										Sqz(
											CAS(Var("Tail"), Var("t"), Var("n"), use_age_fields)
										)
									),
									Brk()
								)
							)
						)
					)
				),
				Sqz(ChkP(false))
			),
			Kill("h"),
			Kill("t"),
			Kill("n"),
			Kill()
		)));

		#if REPLACE_INTERFERENCE_WITH_SUMMARY
			// atomic {
			// 	t = Tail.next;
			// 	if (t == NULL) {
			// 		aux = malloc;
			// 		aux.data = __in__;
			// 		aux.next = NULL;
			// 		Tail.next = aux *** enq(__in__) ***;
			// 	} else {
			// 		Tail = t;
			// 	}
			// }
			auto enqsum = AtomicSqz(
				Assign(Var("t"), Next("Tail")),
				IfThenElse(
					EqCond(Var("t"), Null()),
					Sqz(
						Mllc("n"),
						SetNull(Next("n")),
						Read("n"),
						Assign(Next("Tail"), Var("n"), LinP())
					),
					Sqz(
						Assign(Var("Tail"), Var("t"))
					)
				)
			);

			//// atomic {
			//// 	n = Head.next;
			//// 	if (Head == Tail) {
			//// 		if (n == NULL) {
			//// 			*** deq(empty) ***
			//// 		} else {
			//// 			Tail = n;
			//// 		}
			//// 	} else {
			////		Head = n *** deq(n.data) ***
			//// 	}
			//// }
			//
			// atomic {
			// 	n = Head.next;
			// 	if (n == NULL) {
			// 		*** deq(empty) ***
			// 	} else {
			//		if (*) {
			//			if (Tail.next == NULL) {
			//			Tail = Tail.next;
			//		} else {
			// 			Head = n *** deq(n.data) ***
			//		}
			// }
			auto deqsum = AtomicSqz(
				Assign(Var("n"), Next("Head")),
				IfThenElse(
					EqCond(Var("n"), Null()),
					Sqz(LinP()),
					Sqz(
						IfThenElse(
							NDCond(),
							Sqz(
								Assign(Var("t"), Next("Tail")),
								IfThen(
									NeqCond(Var("t"), Null()),
									Sqz(Assign(Var("Tail"), Var("t")))
								)
							),
							Sqz(Assign(Var("Head"), Var("n"), LinP("n")))
						)
					)
				)
			);
		#endif


		std::string name = "DGLMQueue";

		return Prog(
			name,
			{"Head", "Tail"},
			{"h", "t", "n"},
			std::move(init),
			#if REPLACE_INTERFERENCE_WITH_SUMMARY
				Fun("enq", true, std::move(enqbody), std::move(enqsum)),
				Fun("deq", false, std::move(deqbody), std::move(deqsum))
			#else
				Fun("enq", true, std::move(enqbody)),
				Fun("deq", false, std::move(deqbody))
			#endif
		);
	}

	std::unique_ptr<Program> micheal_scott_queue() {
		// original Micheal&Scott queue
		return micheal_scott_queue(false, false, true);
	}

	std::unique_ptr<Program> dglm_queue() {
		// original Micheal&Scott queue
		return dglm_queue(false, false, true);
	}

}