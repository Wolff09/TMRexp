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
		Mllc("node"),
		SetNull(Next("node")),
		Read("node"),
		AtomicSqz(
			LinP(),
			Assign(Next("Tail"), Var("node")),
			Assign(Var("Tail"), Var("node"))
		)
	);

	// deq
	auto deqbody = Sqz(
		AtomicSqz(
			Assign(Var("node"), Next("Head")),
			IfThenElse(
				EqCond(Var("node"), Null()),
				Sqz(
					LinP()
				),
				Sqz(
					LinP("node"),
					Write("node"),
					Assign(Var("copy1"), Var("Head")),
					Assign(Var("Head"), Var("node")),
					Fr("copy1")
				)
			)
		)
	);

	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		// enq summary
		auto enqsum = AtomicSqz(
			Mllc("node"),
			SetNull(Next("node")),
			Read("node"),
			LinP(),
			Assign(Var("copy1"), Next("Tail")),
			CAS(Next("Tail"), Var("copy1"), Var("node"), false),
			ChkReach("copy1"),
			Assign(Var("copy2"), Var("Tail")),
			CAS(Var("Tail"), Var("copy2"), Var("node"), false),
			ChkReach("copy2")
		);

		// deq summary
		auto deqsum = AtomicSqz(
			Assign(Var("node"), Next("Head")),
			IfThenElse(
				EqCond(Var("node"), Null()),
				Sqz(
					LinP()
				),
				Sqz(
					LinP("node"),
					Write("node"),
					Assign(Var("copy1"), Var("Head")),
					CAS(Var("Head"), Var("Head"), Var("node"), false),
					Fr("copy1")
				)
			)
		);
	#endif

	std::string name = "CoarseQueue";

	auto prog = Prog(
		name,
		{"Head", "Tail"},
		{"node", "copy1", "copy2"},
		std::move(init),
		#if REPLACE_INTERFERENCE_WITH_SUMMARY
			Fun("enq", true, std::move(enqbody), std::move(enqsum)),
			Fun("deq", false, std::move(deqbody), std::move(deqsum))
		#else
			Fun("enq", true, std::move(enqbody)),
			Fun("deq", false, std::move(deqbody))
		#endif
	);

	prog->set_chk_mimic_precision(true);

	return prog;
}


int main(int argc, char *argv[]) {
	// make program and observer
	std::unique_ptr<Program> program = mk_program();
	std::unique_ptr<Observer> observer = queue_observer(find(*program, "enq"), find(*program, "deq"), program->freefun());

	return run(*program, *observer);
}

