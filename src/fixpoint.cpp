#include "fixpoint.hpp"

#include <set>
#include "fixp/cfgpost.hpp"
#include "fixp/interference.hpp"
#include "counter.hpp"
#include "config.hpp"
#include "chkmimic.hpp"

using namespace tmr;


/******************************** INITIAL CFG ********************************/

Cfg mk_init_cfg(const Program& prog, const Observer& obs, MemorySetup msetup) {
	std::size_t numThreads = msetup == MM ? 2 : 1;
	Cfg init(
		{{ &prog.init(), NULL, NULL }},
		obs.initial_state(),
		new Shape(obs.numVars(), prog.numGlobals(), prog.numLocals(), numThreads),
		new AgeMatrix(prog.numGlobals() + obs.numVars(), prog.numLocals(), numThreads),
		MultiInOut()
	);
	while (init.pc[0] != NULL) {
		std::vector<Cfg> postpc = tmr::post(init, 0, msetup);
		assert(postpc.size() == 1);
		init = std::move(postpc.front());
	}
	assert(init.pc[0] == NULL);
	assert(init.pc[1] == NULL);
	assert(init.pc[2] == NULL);
	assert(init.shape);
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
	#if !REPLACE_INTERFERENCE_WITH_SUMMARY
		if (cfg.state.is_final()) return;
	#endif
	auto res = _enc.take(std::move(cfg));
	if (res.first) _work.insert(&res.second);
}


/******************************** FIXED POINT ********************************/

std::unique_ptr<Encoding> tmr::fixed_point(const Program& prog, const Observer& obs, MemorySetup msetup) {
	std::unique_ptr<Encoding> enc = std::make_unique<Encoding>();

	RemainingWork work(*enc);
	work.add(mk_init_cfg(prog, obs, msetup));
	assert(!work.done());


	#if REPLACE_INTERFERENCE_WITH_SUMMARY && USE_MODIFIED_FIXEDPOINT
		
		/* FIXED POINT WITH SUMMARIES */
		/* Since summaries are independent of the current encoding, we
		 * need to apply them only once like a sequential step
		 */
		std::size_t counter = 0;

		std::cerr << "combined post...     ";
		while (!work.done()) {
			const Cfg& topmost = work.pop();

			work.add(tmr::mk_all_post(topmost, prog, msetup));
			mk_summary(work, topmost, prog, msetup);

			counter++;
			if (counter%1000 == 0) std::cerr << "[" << counter/1000 << "k-" << enc->size()/1000 << "k-" << work.size()/1000 << "k]";
		}
		std::cerr << " done! [enc.size()=" << enc->size() << ", iterations=" << counter << "]" << std::endl;

	#else

		/* FIXED POINT WITH INTERFERENCE */
		while (!work.done()) {
			std::size_t counter = 0;

			std::cerr << "post image...     ";
			while (!work.done()) {
				const Cfg& topost = work.pop();
				SEQUENTIAL_STEPS++;
				work.add(tmr::mk_all_post(topost, prog, msetup));
				
				counter++;
				if (counter%10000 == 0) std::cerr << "[" << counter/1000 << "k-" << enc->size()/1000 << "k]";
			}
			std::cerr << " done! [enc.size()=" << enc->size() << ", iterations=" << counter << "]" << std::endl;

			tmr::mk_all_interference(*enc, work, msetup);
		}

	#endif

	std::string ainfo = "";

	#if REPLACE_INTERFERENCE_WITH_SUMMARY && SUMMARY_CHKMIMIC
		std::cerr << "chk_mimic...        ";
		if (chk_mimic(*enc)) {
			std::cerr << " done! [effects=" << SUMMARIES_NEEDED << "]" << std::endl;
			ainfo = " Approximation proven sound.";
		} else {
			std::cerr << " failed!" << std::endl;
			throw std::runtime_error("Misbehaving Summary: CHK-MIMIC failed.");
		}
	#endif

	std::cout << std::endl << "Fixed point computed " << enc->size() << " distinct configurations." << ainfo << std::endl;
	return enc;
}
