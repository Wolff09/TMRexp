#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
	// init prog
	auto init = Sqz(
		Mllc("HPrecs"),
		SetNull(Next("HPrecs")),
		Assign(Var("Tail"), Var("HPrecs"))
	);

	// init thread
	auto initthread = Sqz(
		Mllc("cur"),
		SetNull(Next("cur")),
		Loop(Sqz(
			Assign(Var("tmp"), Var("Tail")),
			Assign(Var("next"), Next("tmp")),
			IfThenElse(
				EqCond(Var("next"), Null()),
				Sqz(
					IfThen(
						CasCond(CAS(Next("Tail"), Var("next"), Var("cur"))),
						Sqz(
							CAS(Var("Tail"), Var("tmp"), Var("cur")),
							Brk()
						)
					)
				),
				Sqz(
					CAS(Var("Tail"), Var("tmp"), Var("next"))
				)
			),
			Kill("tmp"),
			Kill("next")
		)),
		InitRec("cur"),
		WriteRecNull(0),
		WriteRecNull(1),
		Kill("cur"),
		Kill("tmp"),
		Kill("next")
	);

	// protect
	auto protect0 = Sqz(
		WriteRecArg(0)
	);
	auto protect1 = Sqz(
		WriteRecArg(1)
	);

	// unprotect
	auto unprotect0 = Sqz(
		WriteRecNull(0)
	);
	auto unprotect1 = Sqz(
		WriteRecNull(1)
	);

	// retire
	auto retire = Sqz(
		AddArg(0),
		IfThen(
			NDCond(),
			Sqz(
				Assign(Var("cur"), Next("HPrecs")),
				Loop(Sqz(
					IfThenElse(
						EqCond(Var("cur"), Null()),
						Sqz(Brk()),
						Sqz(
							AddSel(1, Data("cur", 0)),
							AddSel(1, Data("cur", 1)),
							Assign(Var("tmp"), Next("cur")),
							Assign(Var("cur"), Var("tmp")),
							Kill("tmp")
						)
					)
				)),
				SetAssign(2, 0),
				SetMinus(2, 1),
				Free(2),
				SetMinus(0, 2),
				Clear(1),
				Clear(2),
				Kill("cur")
			)
		)
	);


	std::string name = "HazardPointerImpl_Queue";

	auto prog = Prog(
		name,
		{"HPrecs", "Tail"},
		{"cur", "tmp", "next"},
		std::move(init),
		std::move(initthread),
		Fun("protect0", std::move(protect0), true),
		Fun("protect1", std::move(protect1), true),
		Fun("unprotect0", std::move(unprotect0), false),
		Fun("unprotect1", std::move(unprotect1), false),
		Fun("retire", std::move(retire), true)
	);

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	return run_hp(*program);
}
