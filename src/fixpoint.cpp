#include "fixpoint.hpp"

#include <set>
#include "fixp/cfgpost.hpp"
#include "fixp/interference.hpp"
#include "counter.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** INITIAL CFG ********************************/

void add_init_cfg(const Program& prog, const Observer& smrobs, const Observer& threadobs, RemainingWork& work) {
	std::size_t numThreads = 1;

	// initial shape
	Shape* initial_shape = new Shape(prog.numGlobals(), prog.numLocals(), numThreads);

	// initial cfg
	Cfg init(
		{{ &prog.init(), NULL }},
		smrobs.initial_state(),
		threadobs.initial_state(),
		initial_shape
	);

	// execute init()
	while (init.pc[0] != NULL) {
		std::vector<Cfg> postpc = tmr::post(init, 0);
		init = std::move(postpc.front());
	}

	// prepare to execute threadinit()
	init.pc[0] = &prog.init_thread();
	init.offender[0] = false;

	Cfg copy = init.copy();
	copy.offender[0] = true;

	// done
	work.add(std::move(init));
	work.add(std::move(copy));
}


/******************************** WORK SET ********************************/

RemainingWork::RemainingWork(Encoding& enc) : _enc(enc) {}

const Cfg& RemainingWork::pop() {
	const Cfg* top = *_work.begin();
	_work.erase(_work.begin());
	return *top;
}

void RemainingWork::add(Cfg&& cfg) {
	if (cfg.smrstate.is_marked() || cfg.threadstate[0].is_marked() || cfg.threadstate[1].is_marked()) {
		return;
	}

	// if (cfg.globalepoch == Epoch::TWO && cfg.epochsel == Epoch::ZERO && cfg.offender[0]) throw std::logic_error("epochsel lacking behind");

	// std::cout << "adding: " << cfg << std::endl;
	auto res = _enc.take(std::move(cfg));
	if (res.first) _work.insert(&res.second);
}


/******************************** FIXED POINT ********************************/

std::unique_ptr<Encoding> tmr::fixed_point(const Program& prog, const Observer& smrobs, const Observer& threadobs) {
	std::unique_ptr<Encoding> enc = std::make_unique<Encoding>();

	RemainingWork work(*enc);
	add_init_cfg(prog, smrobs, threadobs, work);


	while (!work.done()) {
		std::size_t counter = 0;
		std::cerr << "post image...     [#enc]";

		// sequential steps
		while (!work.done()) {
			const Cfg& topost = work.pop();
			// std::cout << std::endl << std::endl << "==============================================================" << std::endl << "posting: " << topost; // DEBUG OUTPUT
			// if (topost.pc[0] && topost.pc[0]->id() == 32) std::cout << *topost.shape << std::endl; // DEBUG OUTPUT
			// std::cout << std::endl; // DEBUG OUTPUT
			work.add(tmr::mk_all_post(topost, prog));

			SEQUENTIAL_STEPS++;
			counter++;
			if (counter%1000 == 0) {
				std::cerr << "[" << enc->size()/1000 << "k]" << std::flush;
			}
		}
		std::cerr << " done! [#enc=" << enc->size()/1000 << "." << (enc->size()-((enc->size()/1000)*1000))/100 << "k";
		std::cerr << ", #step=" << counter/1000 << "k";
		std::cerr << ", #steptotal=" << SEQUENTIAL_STEPS/1000 << "k]" << std::endl;

		// interference steps
		tmr::mk_all_interference(*enc, work, prog);
	}

	return enc;
}
