#include "run.hpp"

using namespace tmr;


bool DGLMhint(void* param) {
	Shape* shape = (Shape*) param;
	shape->remove_relation(6, 5, GT);
	return shape->at(6, 5).none();
}

static std::unique_ptr<Program> mk_program() {
	bool use_age_fields = false;
	bool age_compare = false;

	// init
	std::unique_ptr<Sequence> init = Sqz(
		Mllc("Head"),
		SetNull(Next("Head")),
		Assign(Var("Tail"), Var("Head"))
	);

	// enq
	auto enqbody = Sqz(
		Enter(),
		Mllc("h"),
		SetNull(Next("h")),
		Read("h"),
		Loop(Sqz(
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
			Kill("n"),
			Kill("t")//,
			// SetNull(Next("h"))
		)),
		CAS(Var("Tail"), Var("t"), Var("h"), use_age_fields),
		Leave()
	);

	// deq
	auto linpc = CompCond(OCond(), CompCond(EqCond(Var("h"), Var("Head"), age_compare), EqCond(Var("n"), Null())));
	auto deqbody = Sqz(Loop(Sqz(
		Enter(),
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
								// Fr("h"),
								Rtire("h"),
								Kill("t"),
								Brk()
							)
						)
					)
				)
			),
			Sqz(ChkP(false))
		),
		Kill("n"),
		Kill("h"),
		Kill(),
		Leave()
	)));

	std::string name = "DGLMQueue";

	auto prog = Prog(
		name,
		{"Head", "Tail"},
		{"h", "t", "n"},
		std::move(init),
		Fun("enq", true, std::move(enqbody)),
		Fun("deq", false, std::move(deqbody))
	);

	prog->smr_observer(ebr_observer(prog->enterfun(), prog->leavefun(), prog->retirefun(), prog->freefun()));
	// prog->set_hint(&DGLMhint);

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> observer = queue_observer(find(*program, "enq"), find(*program, "deq"), program->freefun());

	return run(*program, *observer);
}
