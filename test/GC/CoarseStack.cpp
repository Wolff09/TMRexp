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
					Rtire("top")
				)
			)
		)
	);

	std::string name = "CoarseStack";

	auto prog = Prog(
		name,
		{"TopOfStack"},
		{"node", "top"},
		std::move(init),
		Fun("push", true, std::move(pushbody)),
		Fun("pop", false, std::move(popbody))
	);

	prog->smr_observer(no_reclamation_observer(prog->freefun()));

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> observer = stack_observer(find(*program, "push"), find(*program, "pop"), program->freefun());

	return run(*program, *observer);
}
