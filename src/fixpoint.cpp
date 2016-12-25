#include "fixpoint.hpp"

#include <set>
#include "fixp/cfgpost.hpp"
#include "fixp/interference.hpp"
#include "counter.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** INITIAL CFG ********************************/

Cfg mk_init_cfg(const Program& prog, const Observer& obs) {
	std::size_t numThreads = 1;
	MultiPc pc;
	pc[0] = &prog.init();
	pc[1] = NULL;
	Cfg init(
		pc,
		obs.initial_state(),
		new Shape(obs.numVars(), prog.numGlobals(), prog.numLocals(), numThreads),
		MultiInOut()
	);
	while (init.pc[0] != NULL) {
		std::vector<Cfg> postpc = tmr::post(init, 0);
		assert(postpc.size() == 1);
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
	//std::lock_guard<std::mutex> guard(_mut);
	#if !REPLACE_INTERFERENCE_WITH_SUMMARY
		if (cfg.state.is_final()) return;
	#endif
	auto res = _enc.take(std::move(cfg));
	if (res.first) _work.insert(&res.second);
}


/******************************** FIXED POINT ********************************/

std::unique_ptr<Encoding> tmr::fixed_point(const Program& prog, const Observer& obs) {
	std::unique_ptr<Encoding> enc = std::make_unique<Encoding>();

	RemainingWork work(*enc);
	work.add(mk_init_cfg(prog, obs));


	#if REPLACE_INTERFERENCE_WITH_SUMMARY && USE_MODIFIED_FIXEDPOINT
		
		/* FIXED POINT WITH SUMMARIES */
		/* Since summaries are independent of the current encoding, we
		 * need to apply them only once like a sequential step
		 */
		std::size_t counter = 0;

		std::cerr << "combined post...     ";
		while (!work.done()) {
			const Cfg& topmost = work.pop();

			work.add(tmr::mk_all_post(topmost, prog));
			mk_summary(work, topmost, prog);

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
				work.add(tmr::mk_all_post(topost, prog));
				
				counter++;
				if (counter%10000 == 0) std::cerr << "[" << counter/1000 << "k-" << enc->size()/1000 << "k]";
			}
			std::cerr << " done! [enc.size()=" << enc->size() << ", iterations=" << counter << "]" << std::endl;

			tmr::mk_all_interference(*enc, work);
		}

	#endif


	std::cout << std::endl << "Fixed point computed " << enc->size() << " distinct configurations." << std::endl;
	return enc;
}
