#pragma once

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
		// stmts.push_back(SetNull(Next(vname)));
		stmts.push_back(std::make_unique<ReadInputAssignment>(Data(vname)));
		return std::make_unique<Atomic>(std::make_unique<Sequence>(std::move(stmts)));
	}

	static std::unique_ptr<Program> treibers_stack(bool cheating_cas, bool mega_malloc, bool use_age_fields) {
		/* Program */

		// TopOfStack = NULL;
		std::unique_ptr<Sequence> init = Sqz(
			SetNull(Var("TopOfStack"))
		);


		// malloc(node);
		// node.data = __in__;
		// // vs.
		// atomic {
		//    malloc(node);
		//    node.next = NULL;
		//    node.data = __in__;
		// }
		// 
		// while (true) {
		//     top = TopOfStack;
		//     node.next = top;
		//     if (CAS(TopOfStack, top, node)) *** push(__in__) *** {
		//         break;
		//     }
		// }
		std::unique_ptr<While> pushloop = Loop(Sqz(
			Assign(Var("top"), Var("TopOfStack")),
			Assign(Next("node"), Var("top")),
			IfThen(
				CasCond(CAS(Var("TopOfStack"), Var("top"), Var("node"), LinP(), use_age_fields)),
				Sqz(Brk())
			)
		));
		std::unique_ptr<Sequence> pushbody;
		if (mega_malloc) {
			pushbody = Sqz(
				mk_mega_malloc("node"),
				std::move(pushloop)
			);
		} else {
			pushbody = Sqz(
				Mllc("node"),
				Read("node"),
				std::move(pushloop)
			);
		}
		assert(pushbody);


		// while (true) {
		//     top = TopOfStack *** [top == NULL] pop(empty) ***;
		//     if (top == NULL) {
		//         __out__ = empty;
		//         break;
		//     } else {
		//         node = top.next
		//         if (CAS(TopOfStack, top, node)) *** pop(top.data) *** {
		//         // vs.
		//         if (CAS(TopOfStack, top, top.next)) *** pop(top.data) *** {
		//             __out__ = top.data;
		//             free(top);
		//             break;
		//         }
		//     }
		// }
		std::unique_ptr<Sequence> popelseif = Sqz(
			Write("top"),
			Brk()
		);
		std::unique_ptr<Sequence> popelse;
		if (cheating_cas) {
			popelse = Sqz(
				IfThen(
					CasCond(CAS(Var("TopOfStack"), Var("top"), Next("top"), LinP("top"), use_age_fields)),
					std::move(popelseif)
				)
			);
		} else {
			popelse = Sqz(
				Assign(Var("node"),	Next("top")),
				IfThen(
					CasCond(CAS(Var("TopOfStack"), Var("top"), Var("node"), LinP("top"), use_age_fields)),
					std::move(popelseif)
				)
			);
		}
		assert(popelse);
		std::unique_ptr<Sequence> popbody = Sqz(Loop(Sqz(
			Assign(Var("top"), Var("TopOfStack"), LinP(EqCond(Var("top"), Null()))),
			IfThenElse(
				EqCond(Var("top"), Null()),
				Sqz(Brk()),
				std::move(popelse)
			)
		)));

		#if REPLACE_INTERFERENCE_WITH_SUMMARY
			// atmoic {
			// 	aux = malloc();
			// 	aux.data = __in__;
			// 	aux.next = TopOfStack;
			// 	TopOfStack = aux *** push(__in__) ***;
			// }
			auto pushsum = AtomicSqz(
					Mllc("node"),
					Read("node"),
					Assign(Next("node"), Var("TopOfStack")),
					Assign(Var("TopOfStack"), Var("node"), LinP())
			);

			// atomic {
			//	if (TopOfStack == NULL) {
			//		*** pop(empty) ***
			//	} else {
			//	 	aux = TopOfStack;
			//	 	TopOfStack = aux.next *** pop(aux) ***;
			//	}
			// }
			auto popsum = AtomicSqz(IfThenElse(
				EqCond(Var("TopOfStack"), Null()),
				Sqz(LinP()),
				Sqz(
					Assign(Var("top"), Var("TopOfStack")),
					Assign(Var("TopOfStack"), Next("top")),
					LinP("top")
				)
			));
		#endif

		std::string name = "TreibersStack";

		return Prog(
			name,
			{"TopOfStack"},
			{"node", "top"},
			std::move(init),
			#if REPLACE_INTERFERENCE_WITH_SUMMARY
				Fun("push", true, std::move(pushbody), std::move(pushsum)),
				Fun("pop", false, std::move(popbody), std::move(popsum))
			#else
				Fun("push", true, std::move(pushbody)),
				Fun("pop", false, std::move(popbody))
			#endif
		);
	}

	std::unique_ptr<Program> treibers_stack(bool use_age_fields) {
		// original Treibers uses non-HW "cheating" CAS
		return treibers_stack(true, false, use_age_fields);
	}

}