#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
	bool use_age_fields = false;

	// init
	auto init = Sqz(
		SetNull(Var("TopOfStack"))
	);

	// push
	auto pushbody = Sqz(
		Enter(),
		Mllc("node"),
		Read("node"),
		Loop(Sqz(
			Assign(Var("top"), Var("TopOfStack")),
			Assign(Next("node"), Var("top")),
			IfThen(
				CasCond(CAS(Var("TopOfStack"), Var("top"), Var("node"), LinP(), use_age_fields)),
				Sqz(Brk())
			),
			Kill("top")
		)),
		Leave()
	);

	// pop
	auto popbody = Sqz(Loop(Sqz(
		Enter(),
		Assign(Var("top"), Var("TopOfStack"), LinP(EqCond(Var("top"), Null()))),
		IfThenElse(
			EqCond(Var("top"), Null()),
			Sqz(Brk()),
			Sqz(
				Assign(Var("node"),	Next("top")),
				IfThen(
					CasCond(CAS(Var("TopOfStack"), Var("top"), Var("node"), LinP("top"), use_age_fields)),
					Sqz(
						Write("top"),
						Rtire("top"),
						// UGard(0),
						Brk()
					)
				),
				Kill("node")
			)
		),
		Kill("top"),
		Leave()
	)));

	std::string name = "TreibersStack";

	auto prog = Prog(
		name,
		{"TopOfStack"},
		{"node", "top"},
		std::move(init),
		Fun("push", true, std::move(pushbody)),
		Fun("pop", false, std::move(popbody))
	);

	prog->smr_observer(ebr_observer(prog->enterfun(), prog->leavefun(), prog->retirefun(), prog->freefun()));

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> linobserver = stack_observer(find(*program, "push"), find(*program, "pop"), program->freefun());

	return run(*program, *linobserver);
}
