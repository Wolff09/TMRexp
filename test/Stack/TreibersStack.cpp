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
		Mllc("node"),
		Read("node"),
		Loop(Sqz(
			Assign(Var("top"), Var("TopOfStack")),
			Gard("top", 0),
			IfThen(
				EqCond(Var("top"), Var("TopOfStack")),
				Sqz(
					Assign(Next("node"), Var("top")),
					IfThen(
						CasCond(CAS(Var("TopOfStack"), Var("top"), Var("node"), LinP(), use_age_fields)),
						Sqz(Brk())
					)
				)
			),
			Kill("top")
		))
	);

	// pop
	auto popbody = Sqz(Loop(Sqz(
		Assign(Var("top"), Var("TopOfStack"), LinP(EqCond(Var("top"), Null()))),
		IfThenElse(
			EqCond(Var("top"), Null()),
			Sqz(Brk()),
			Sqz(
				Gard("top", 0),
				IfThen(
					EqCond(Var("top"), Var("TopOfStack")),
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
				)//,
				//UGard(0)
			)
		),
		Kill("top")
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

	prog->smr_observer(smr_observer(prog->guardfun(), prog->unguardfun(), prog->retirefun(), prog->freefun()));

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> linobserver = stack_observer(find(*program, "push"), find(*program, "pop"), program->freefun());

	return run(*program, *linobserver);
}
