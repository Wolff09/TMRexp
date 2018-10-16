#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
	bool use_age_fields = false;

	// init prog
	auto init = Sqz(
		SetNull(Var("HPrecs"))
	);

	// init thread
	auto initthread = Sqz(
		Mllc("rec0"),
		Mllc("rec1"),
		// TODO: set rec0/rec1 data to NULL
		Assign(Next("rec0"), Var("rec1")),
		Loop(Sqz(
			Assign(Var("top"), Var("HPrecs")),
			Assign(Next("rec1"), Var("top")),
			IfThen(
				CasCond(CAS(Var("HPrecs"), Var("top"), Var("rec0"), use_age_fields)),
				Sqz(Brk())
			),
			Kill("top")
		))
	);

	// protect
	auto protect0 = Sqz(
		Read("rec0")
	);
	auto protect1 = Sqz(
		Read("rec1")
	);

	// unprotect
	auto unprotect0 = Sqz(
		SetNull(Data("rec0"))
	);
	auto unprotect1 = Sqz(
		SetNull(Data("rec1"))
	);

	// retire
	auto retire = Sqz(
		AddArg(0),
		// TODO: if (*) scan();
		Free(0)
	);


	std::string name = "HazardPointerImpl";

	auto prog = Prog(
		name,
		{"HPrecs"},
		{"rec0", "rec1", "top"},
		std::move(init),
		std::move(initthread),
		Fun("protect0", std::move(protect0)),
		Fun("protect1", std::move(protect1)),
		Fun("unprotect0", std::move(unprotect0)),
		Fun("unprotect1", std::move(unprotect1)),
		Fun("retire", std::move(retire))
	);

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	return run_hp(*program);
}
