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
					Rtire("copy1")
				)
			)
		)
	);

	std::string name = "CoarseQueue";

	auto prog = Prog(
		name,
		{"Head", "Tail"},
		{"node", "copy1", "copy2"},
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
