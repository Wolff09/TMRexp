#include "run.hpp"

using namespace tmr;


static std::unique_ptr<Program> mk_program() {
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
			Assign(Var("n"), Next("t")),
			IfThen(
				EqCond(Var("t"), Var("Tail"), false),
				Sqz(IfThenElse(
					EqCond(Var("n"), Null()),
					Sqz(
						IfThen(
							// CasCond(CAS(Next("t"), Null(), Var("h"), LinP(), false)),
							CasCond(CAS(Next("t"), Var("n"), Var("h"), LinP(), false)),
							Sqz(Brk())
					)),
					Sqz(CAS(Var("Tail"), Var("t"), Var("n"), false))
				))
			),
			Kill("t"),
			Kill("n"),
			SetNull(Next("h"))
		)),
		CAS(Var("Tail"), Var("t"), Var("h"), false)
	);

	// deq
	auto linpc = CompCond(OCond(), CompCond(EqCond(Var("h"), Var("Head"), false), CompCond(EqCond(Var("h"), Var("t")), EqCond(Var("n"), Null()))));
	auto deqbody = Sqz(Loop(Sqz(
		Assign(Var("h"), Var("Head")),
		Assign(Var("t"), Var("Tail")),
		Orcl(),
		Assign(Var("n"), Next("h"), LinP(std::move(linpc))),
		IfThenElse(
			EqCond(Var("h"), Var("Head"), false),
			Sqz(
				ChkP(true),
				IfThenElse(
					EqCond(Var("h"), Var("t")),
					Sqz(
						IfThen(
							EqCond(Var("n"), Null()),
							Sqz(Brk())
						),
						CAS(Var("Tail"), Var("t"), Var("n"), false)
					),
					Sqz(
						Write("n"),
						IfThen(
							CasCond(CAS(Var("Head"), Var("h"), Var("n"), LinP("n"), false)),
							Sqz(
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
					Assign(Next("Tail"), Var("n"), LinP())
				),
				Sqz(
					Assign(Var("Tail"), Var("t"))
				)
			)
		);

		// deq summary
		auto deqsum = AtomicSqz(
			Assign(Var("n"), Next("Head")),
			IfThenElse(
				EqCond(Var("Head"), Var("Tail")),
				Sqz(
					IfThenElse(
						EqCond(Var("n"), Null()),
						Sqz(LinP()),
						Sqz(Assign(Var("Tail"), Var("n")))
					)
				),
				Sqz(
					Assign(Var("Head"), Var("n"), LinP("n"))
				)
			)
		);
	#endif

	std::string name = "MichealAndScottQueue";

	return Prog(
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
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> observer = queue_observer(find(*program, "enq"), find(*program, "deq"), program->freefun());

	return run(*program, *observer);
}
