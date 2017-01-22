#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
	// init
	auto init = Sqz(
		SetNull(Var("TopOfStack"))
	);

	// push
	auto pushbody = Sqz(
		Mllc("node"),
		Read("node"),
		Loop(Sqz(
			Assign(Var("top"), Var("TopOfStack")),
			Assign(Next("node"), Var("top")),
			IfThen(
				CasCond(CAS(Var("TopOfStack"), Var("top"), Var("node"), LinP(), false)),
				Sqz(Brk())
			)
		))
	);

	// pop
	auto popbody = Sqz(Loop(Sqz(
		Assign(Var("top"), Var("TopOfStack"), LinP(EqCond(Var("top"), Null()))),
		IfThenElse(
			EqCond(Var("top"), Null()),
			Sqz(Brk()),
			Sqz(
				Assign(Var("node"),	Next("top")),
				IfThen(
					CasCond(CAS(Var("TopOfStack"), Var("top"), Var("node"), LinP("top"), false)),
					Sqz(
						Write("top"),
						Brk()
					)
				)
			)
		)
	)));

	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		// push summary
		auto pushsum = AtomicSqz(
				Mllc("node"),
				Read("node"),
				Assign(Next("node"), Var("TopOfStack")),
				Assign(Var("TopOfStack"), Var("node"), LinP())
		);

		// pop summary
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


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> observer = stack_observer(find(*program, "push"), find(*program, "pop"), program->freefun());

	return run(*program, *observer);
}
