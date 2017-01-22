#include "run.hpp"

using namespace tmr;


bool DGLMhint(void* param) {
	Shape* shape = (Shape*) param;
	shape->remove_relation(6, 5, GT);
	return shape->at(6, 5).none();
}

static std::unique_ptr<Program> mk_program() {
	bool use_age_fields = true;
	bool age_compare = true;

	// init
	std::unique_ptr<Sequence> init = Sqz(
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
			Kill("t"),
			Kill("n"),
			SetNull(Next("h"))
		)),
		CAS(Var("Tail"), Var("t"), Var("h"), use_age_fields)
	);

	// deq
	auto linpc = CompCond(OCond(), CompCond(EqCond(Var("h"), Var("Head"), age_compare), EqCond(Var("n"), Null())));
	auto deqbody = Sqz(Loop(Sqz(
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
								Fr("h"),
								Brk()
							)
						)
					)
				)
			),
			Sqz(ChkP(false))
		),
		Kill("h"),
		Kill("t"),
		Kill("n"),
		Kill()
	)));

	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		// enq summary
		auto enqsum = AtomicSqz(
			Assign(Var("t"), Next("Tail")),
			IfThenElse(
				EqCond(Var("t"), Null()),
				Sqz(
					Mllc("n"),
					SetNull(Next("n")),
					Read("n"),
					CAS(Next("Tail"), Var("t"), Var("n"), LinP(), use_age_fields),
					ChkReach("t")
				),
				Sqz(
					Assign(Var("h"), Var("Tail")),
					CAS(Var("Tail"), Var("Tail"), Var("t"), use_age_fields),
					IfThenElse(
						EqCond(Var("Tail"), Var("Head")),
						Sqz(Fr("h")),
						Sqz(ChkReach("h"))
					)
				)
			)
		);

		// deq summary
			auto deqsum = AtomicSqz(
			IfThenElse(
				EqCond(Var("Head"), Var("Tail")),
				Sqz(
					Assign(Var("n"), Next("Head")),
					IfThenElse(
						EqCond(Var("n"), Null()),
						Sqz(
							LinP()
							// IfThenElse(
							// 	NDCond(),
							// 	Sqz(LinP()),
							// 	Sqz(
							// 		CAS(Var("Tail"), Var("Tail"), Var("Head"), use_age_fields)
							// 	)
							// )
						),
						Sqz(
							Assign(Var("h"), Var("Head")),
							CAS(Var("Head"), Var("Head"), Var("n"), LinP("n"), use_age_fields),
							ChkReach("h")
						)
					)
				),
				Sqz(
					IfThenElse(
						EqCond(Next("Tail"), Var("Head")),
						Sqz(
							Assign(Var("t"), Var("Tail")),
							CAS(Var("Tail"), Var("Tail"), Var("Head"), use_age_fields),
							Fr("t") // this gives double free as expected
						),
						Sqz(
							Assign(Var("n"), Next("Head")),
							IfThenElse(
								EqCond(Var("n"), Null()),
								Sqz(LinP()),
								Sqz(
									Assign(Var("h"), Var("Head")),
									CAS(Var("Head"), Var("Head"), Var("n"), LinP("n"), use_age_fields),
									Fr("h")
								)
							)
						)
					)
				)
			)
		);
	#endif

	std::string name = "DGLMQueue";

	auto prog = Prog(
		name,
		{"Head", "Tail"},
		{"h", "t", "n"},
		std::move(init),
		#if REPLACE_INTERFERENCE_WITH_SUMMARY
			Fun("enq", true, std::move(enqbody), std::move(enqsum)),
			Fun("deq", false, std::move(deqbody), std::move(deqsum))
		#else
			Fun("enq", true, std::move(enqbody)),
			Fun("deq", false, std::move(deqbody))
		#endif
	);

	// prog->set_chk_mimic_precision(true);
	prog->set_hint(&DGLMhint);

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> observer = queue_observer(find(*program, "enq"), find(*program, "deq"), program->freefun());

	return run(*program, *observer);
}
