#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
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
			Assign(Var("cur"), Var("HPrecs")),
			Assign(Next("rec1"), Var("cur")),
			IfThen(
				CasCond(CAS(Var("HPrecs"), Var("cur"), Var("rec0"))),
				Sqz(Brk())
			),
			Kill("cur")
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
	// {{ 0 }} = retired node list (rlist)
	// {{ 1 }} = protected node list (plist)
	// {{ 2 }} = to-delete node list (dlist)
	auto retire = Sqz(
		AddArg(0),
		/* if *:
			List<ptr_t> plist;
			Node* cur = HPRec;
			while (cur != NULL) {
				plist.add(cur->ptr)
			}
			List<ptr_t> dlist;
			dlist = rlist - plist;
			rlist = rlist - dlist
			free(dlist);
			rlist = rlist - dlist;
			plist = empty
			dlist = empty
		 */
		IfThen(
			NDCond(),
			Sqz(
				Assign(Var("cur"), Var("HPrecs")),
				Loop(Sqz(
					IfThenElse(
						EqCond(Var("cur"), Null()),
						Sqz(Brk()),
						Sqz(
							AddSel(1, Data("cur")),
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


	std::string name = "HazardPointerImpl";

	auto prog = Prog(
		name,
		{"HPrecs"},
		{"rec0", "rec1", "cur", "tmp"},
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
