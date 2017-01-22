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
		SetNull(Next("node")),
		Read("node"),
		AtomicSqz(
			LinP(),
			Assign(Next("node"), Var("TopOfStack")),
			Assign(Var("TopOfStack"), Var("node"))
		)
	);

	// pop
	auto popbody = Sqz(
		AtomicSqz(
			Assign(Var("top"), Var("TopOfStack")),
			IfThenElse(
				EqCond(Var("top"), Null()),
				Sqz(
					LinP()
				),
				Sqz(
					LinP("top"),
					Write("top"),
					Assign(Var("TopOfStack"), Next("top")),
					Fr("top")
				)
			)
		)
	);

	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		// push summary
		auto pushsum = AtomicSqz(
				Mllc("node"),
				Read("node"),
				Assign(Next("node"), Var("TopOfStack")),
				Assign(Var("top"), Var("TopOfStack")),
				CAS(Var("TopOfStack"), Var("TopOfStack"), Var("node"), LinP(), false),
				ChkReach("top")
		);

		// pop summary
		auto popsum = AtomicSqz(IfThenElse(
			EqCond(Var("TopOfStack"), Null()),
			Sqz(LinP()),
			Sqz(
				Assign(Var("top"), Var("TopOfStack")),
				Assign(Var("node"), Next("TopOfStack")),
				CAS(Var("TopOfStack"), Var("TopOfStack"), Var("node"), LinP("top"), false),
				Fr("top")
			)
		));
	#endif

	std::string name = "CoarseStack";

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
