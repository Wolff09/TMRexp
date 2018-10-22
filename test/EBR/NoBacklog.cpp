#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
	// init prog
	auto init = Sqz(
		SetNull(Var("EBRrecs"))
	);

	// init thread
	auto initthread = Sqz(
		Mllc("cur"),
		Loop(Sqz(
			Assign(Var("tmp"), Var("EBRrecs")),
			Assign(Next("cur"), Var("tmp")),
			IfThen(
				CasCond(CAS(Var("EBRrecs"), Var("tmp"), Var("cur"))),
				Sqz(Brk())
			),
			Kill("tmp")
		)),
		InitRec("cur"),
		GetEpoch(),
		SetEpoch(),
		Kill("cur"),
		Kill("tmp")
	);

	// enterQ
	auto enterQ = Sqz(
	);

	// leaveQ
	auto leaveQ = Sqz(		
		IfThen(
			NDCond(),
			Sqz(
				IfThen(
					EpochCond(),
					Sqz(
						Free(0),
						Clear(0),
						GetEpoch(),
						SetEpoch()
					)
				),
				Assign(Var("cur"), Var("EBRrecs")),
				Loop(Sqz(
					IfThenElse(
						EqCond(Var("cur"), Null()),
						Sqz(
							AtomicSqz(
								IfThenElse(
									EpochCond(),
									Sqz(Kill("tmp")),
									Sqz(Inc())
								)
							),
							Brk()
						),
						Sqz(
							IfThen(
								EpochCond("cur"),
								Sqz(Brk())
							),
							Assign(Var("tmp"), Next("cur")),
							Assign(Var("cur"), Var("tmp")),
							Kill("tmp")
						)
					)
				)),
				Kill("cur")
			)
		)
	);

	// retire
	auto retire = Sqz(
		AddArg(0)
	);


	std::string name = "EBRimpl_1SetFreeCache";

	auto prog = Prog(
		name,
		{"EBRrecs"},
		{"cur", "tmp"},
		std::move(init),
		std::move(initthread),
		Fun("enterQ", std::move(enterQ), false),
		Fun("leaveQ", std::move(leaveQ), false),
		Fun("retire", std::move(retire), true)
	);

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	return run_ebr_with_inv(*program, false);
}
