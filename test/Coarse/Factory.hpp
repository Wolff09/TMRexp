#pragma once

#include <string>
#include <memory>
#include <vector>
#include "make_unique.hpp"
#include "prog.hpp"

namespace tmr {

	static std::unique_ptr<Program> coarse_queue(bool mega_malloc) {
		/* Program */

		
		// malloc(H);
		// H.next = NULL;
		// T = H;
		std::unique_ptr<Sequence> init = Sqz(
			Mllc("H"),
			SetNull(Next("H")),
			Assign(Var("T"), Var("H"))
		);

		// malloc(n);
		// n.next = NULL;
		// n.data = __in__;
		// Atomic {
		//     T.next = n *** enq(__in__) ***;
		//     T = n;
		// }
		std::unique_ptr<Sequence> enqbody;
		if (mega_malloc) {
			enqbody = Sqz(
				AtomicSqz(
					Mllc("n"),
					SetNull(Next("n")),
					Read("n")
				),
				AtomicSqz(
					LinP(),
					Assign(Next("T"), Var("n")),
					Assign(Var("T"), Var("n"))
				)
			);
		} else {
			enqbody = Sqz(
				Mllc("n"),
				SetNull(Next("n")),
				Read("n"),
				AtomicSqz(
					LinP(),
					Assign(Next("T"), Var("n")),
					Assign(Var("T"), Var("n"))
				)
			);
		}


		// Atomic {
		//     n = H.next
		//     if (n == NULL) {
		//         *** deq(empty) ***
		//     } else {
		//         *** deq(n.data) ***
		//         __out__ = n.data;
		//         free(H);
		//         H = n;
		//     }
		// }
		std::unique_ptr<Sequence> deqbody = Sqz(
			AtomicSqz(
				Assign(Var("n"), Next("H")),
				IfThenElse(
					EqCond(Var("n"), Null()),
					Sqz(
						LinP()
					),
					Sqz(
						LinP("n"),
						Write("n"),
						Fr("H"),
						Assign(Var("H"), Var("n"))
					)
				)
			)
		);


		return Prog(
			"CoarseQueue",
			{"H", "T"},
			{"n"},
			std::move(init),
			Fun("enq", true, std::move(enqbody)),
			Fun("deq", false, std::move(deqbody))
		);
	}


	static std::unique_ptr<Program> coarse_stack(bool mega_malloc) {
		/* Program */

		
		// ToS = NULL
		std::unique_ptr<Sequence> init = Sqz(
			SetNull(Var("ToS"))
		);

		// malloc(n);
		// n.data = __in__;
		// Atomic {
		//     n.next = ToS;
		//     ToS = n;
		//     *** enq(__in__) ***;
		// }
		std::unique_ptr<Statement> atm = AtomicSqz(
			LinP(),
			Assign(Next("n"), Var("ToS")),
			Assign(Var("ToS"), Var("n"))
		);
		std::unique_ptr<Sequence> pushbody;
		if (mega_malloc) {
			pushbody = Sqz(
				AtomicSqz(
					Mllc("n"),
					SetNull(Next("n")),
					Read("n")
				),
				std::move(atm)
			);
		} else {
			pushbody = Sqz(
				Mllc("n"),
				SetNull(Next("n")),
				Read("n"),
				std::move(atm)
			);
		}


		// Atomic {
		//     n = H.next
		//     if (n == NULL) {
		//         *** deq(empty) ***
		//     } else {
		//         *** deq(n.data) ***
		//         __out__ = n.data;
		//         free(H);
		//         H = n;
		//     }
		// }
		std::unique_ptr<Sequence> popbody = Sqz(
			AtomicSqz(
				Assign(Var("n"), Var("ToS")),
				IfThenElse(
					EqCond(Var("n"), Null()),
					Sqz(
						LinP()
					),
					Sqz(
						LinP("n"),
						Write("n"),
						Assign(Var("ToS"), Next("n")),
						Fr("n")
					)
				)
			)
		);


		return Prog(
			"CoarseStack",
			{"ToS"},
			{"n"},
			std::move(init),
			Fun("push", true, std::move(pushbody)),
			Fun("pop", false, std::move(popbody))
		);
	}

}