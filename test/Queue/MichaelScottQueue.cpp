#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
	bool use_age_fields = false;
	bool age_compare = false;

	// init
	auto init = Sqz(
		Mllc("Head"),
		SetNull(Next("Head")),
		Assign(Var("Tail"), Var("Head"))
	);

	// enq
	auto enqbody = Sqz(
		Mllc("h"),
		SetNull(Next("h")),
		Read("h"),
		Loop(Sqz(
			Assign(Var("t"), Var("Tail")),
			Gard("t", 0),
			IfThen(
				EqCond(Var("t"), Var("Tail"), age_compare),
				Sqz(
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
					)
				)
			),
			Kill("t"),
			Kill("n"),
			SetNull(Next("h"))
		)),
		CAS(Var("Tail"), Var("t"), Var("h"), use_age_fields)
	);

	// deq
	auto linpc = CompCond(OCond(), CompCond(EqCond(Var("h"), Var("Head"), age_compare), CompCond(EqCond(Var("h"), Var("t")), EqCond(Var("n"), Null()))));
	auto deqbody = Sqz(Loop(Sqz(
		Assign(Var("h"), Var("Head")),
		Gard("h", 0),
		IfThen(
			EqCond(Var("h"), Var("Head"), age_compare),
			Sqz(
				Assign(Var("t"), Var("Tail")),
				Orcl(),
				Assign(Var("n"), Next("h"), LinP(std::move(linpc))),
				Gard("n", 1),
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
										// Fr("h"),
										Brk()
									)
								)
							)
						)
					),
					Sqz(ChkP(false))
				)
			)
		),
		Kill("h"),
		Kill("t"),
		Kill("n"),
		Kill()
	)));

	std::string name = "MichealAndScottQueue";

	auto prog = Prog(
		name,
		{"Head", "Tail"},
		{"h", "t", "n"},
		std::move(init),
		Fun("enq", true, std::move(enqbody)),
		Fun("deq", false, std::move(deqbody))
	);

	prog->smr_observer(smr_observer(prog->guardfun(), prog->unguardfun(), prog->retirefun(), prog->freefun()));

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> observer = queue_observer(find(*program, "enq"), find(*program, "deq"), program->freefun());

	return run(*program, *observer);
}
