#include "fixpoint.hpp"

#include <set>
#include "fixp/cfgpost.hpp"
#include "fixp/interference.hpp"
#include "counter.hpp"
#include "config.hpp"
#include "chkmimic.hpp"

using namespace tmr;


/******************************** INITIAL CFG ********************************/

Cfg mk_init_cfg(const Program& prog, const Observer& linobs, const Observer& smrobs) {
	std::size_t numThreads = 1;

	auto smrraw = smrobs.initial_state().states();
	if (smrraw.size() != 1) throw std::logic_error("Unexpected form of SMR observer.");
	const State& smrinitial = *smrraw.front();

	Cfg init(
		{{ &prog.init(), NULL, NULL }},
		linobs.initial_state(),
		smrinitial,
		new Shape(linobs.numVars(), prog.numGlobals(), prog.numLocals(), numThreads)
		// TODO: SMR observer
		// MultiInOut()
	);
	while (init.pc[0] != NULL) {
		std::vector<Cfg> postpc = tmr::post(init, 0);
		init = std::move(postpc.front());
	}
	return init;
}


/******************************** WORK SET ********************************/

RemainingWork::RemainingWork(Encoding& enc) : _enc(enc) {}

const Cfg& RemainingWork::pop() {
	const Cfg* top = *_work.begin();
	_work.erase(_work.begin());
	return *top;
}

void RemainingWork::add(Cfg&& cfg) {
	auto res = _enc.take(std::move(cfg));
	if (res.first) _work.insert(&res.second);
}


/******************************** FIXED POINT ********************************/

std::unique_ptr<Encoding> tmr::fixed_point(const Program& prog, const Observer& linobs, const Observer& smrobs) {
	std::unique_ptr<Encoding> enc = std::make_unique<Encoding>();

	RemainingWork work(*enc);
	work.add(mk_init_cfg(prog, linobs, smrobs));
	assert(!work.done());

	while (!work.done()) {
		std::size_t counter = 0;

		std::cerr << "post image...     ";
		while (!work.done()) {
			const Cfg& topost = work.pop();
			SEQUENTIAL_STEPS++;
			work.add(tmr::mk_all_post(topost, prog));
			
			counter++;
			if (counter%10000 == 0) std::cerr << "[" << counter/1000 << "k-" << enc->size()/1000 << "k]";
		}
		std::cerr << " done! [enc.size()=" << enc->size() << ", iterations=" << counter << "]" << std::endl;

		tmr::mk_all_interference(*enc, work);
	}

	std::cout << std::endl << "Fixed point computed " << enc->size() << " distinct configurations." << std::endl;
	return enc;
}
