#pragma once

#include <stdexcept>
#include <string>
#include <memory>
#include <vector>
#include "make_unique.hpp"
#include "prog.hpp"

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
									Fr("h"),
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



		std::string name = "MichealScottQueue";

		return Prog(
			name,
			{"Head", "Tail"},
			{"h", "t", "n"},
			std::move(init),
			Fun("enq", true, std::move(enqbody)),
			Fun("deq", false, std::move(deqbody))
		);
	}

	std::unique_ptr<Program> micheal_scott_queue() {
		// original Micheal&Scott queue
		return micheal_scott_queue(false, false, true);
	}

}