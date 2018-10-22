#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
	// init prog
	auto init = Sqz(
		SetNull(Var("HPrecs"))
	);

	// init thread
	auto initthread = Sqz(
		Mllc("cur"),
		Loop(Sqz(
			Assign(Var("tmp"), Var("HPrecs")),
			Assign(Next("cur"), Var("tmp")),
			IfThen(
				CasCond(CAS(Var("HPrecs"), Var("tmp"), Var("cur"))),
				Sqz(Brk())
			),
			Kill("tmp")
		)),
		InitRec("cur"),
		WriteRecNull(0),
		WriteRecNull(1),
		Kill("cur"),
		Kill("tmp")
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
				Assign(Var("cur"), Var("HPrecs")),
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


	std::string name = "HazardPointerImpl_Stack";

	auto prog = Prog(
		name,
		{"HPrecs"},
		{"cur", "tmp"},
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
