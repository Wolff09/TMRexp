#include "fixpoint.hpp"

#include <set>
#include "fixp/cfgpost.hpp"
#include "fixp/interference.hpp"
#include "counter.hpp"
#include "config.hpp"
#include "chkaware.hpp"

using namespace tmr;


/******************************** INITIAL CFG ********************************/

Cfg mk_init_cfg(const Program& prog, const Observer& linobs) {
	std::size_t numThreads = 1;

	Cfg init(
		{{ &prog.init(), NULL, NULL }},
		linobs.initial_state(),
		new Shape(linobs.numVars(), prog.numGlobals(), prog.numLocals(), numThreads)
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

static inline bool needs_splitting(RelSet rs) {
	return rs != EQ_ && rs != MT_  && rs != GT_  && rs != MT_GT && rs != MF_GF && rs != MF_ && rs != GF_ && rs != BT_;
}

void RemainingWork::add(Cfg&& cfg) {
	#if DGLM_PRECISION
		if (needs_splitting(cfg.shape->at(5,6))) {
			for (RelSet rs : { EQ_, MT_GT, MF_GF, BT_ }) {
				Shape* split = isolate_partial_concretisation(*cfg.shape, 5, 6, rs);
				if (split) add(Cfg(cfg, split));
			}
			return;
		}
	#endif

	auto res = _enc.take(std::move(cfg));
	if (res.first) _work.insert(&res.second);
}

// #if DGLM_PRECISION
// 	void RemainingWork::add(Cfg&& cfg) {
// 		static std::vector<Shape*> worklist;
// 		worklist.reserve(32);
// 		worklist.clear();
// 		// returns true iff the input cfg was split up

// 		auto begin = cfg.shape->offset_program_vars();
// 		auto end = cfg.shape->offset_locals(0);

// 		// std::vector<Shape*> worklist;
// 		worklist.reserve(32);
// 		worklist.push_back(cfg.shape.release());

// 		while (!worklist.empty()) {
// 			Shape* shape = worklist.back();
// 			worklist.pop_back();

// 			for (std::size_t i = begin; i < end; i++) {
// 				for (std::size_t j = i+1; j < end; j++) {
// 					if (!needs_splitting(shape->at(i, j))) continue;
// 					for (RelSet rs : { EQ_, MT_GT, MF_GF, BT_ }) {
// 						Shape* update = new Shape(*shape);
// 						update->set(i, j, intersection(update->at(i, j), rs));
// 						worklist.push_back(update);
// 					}
// 					goto ctn;
// 				}
// 			}

// 			// work.add(Cfg(cfg, shape));
// 			{
// 				auto res = _enc.take(Cfg(cfg, shape));
// 				if (res.first) _work.insert(&res.second);
// 				continue;
// 			}

// 			ctn:; // continue here if the shape was split
// 			delete shape;
// 		}
// 	}
// #endif


/******************************** FIXED POINT ********************************/

std::unique_ptr<Encoding> tmr::fixed_point(const Program& prog, const Observer& linobs) {
	std::unique_ptr<Encoding> enc = std::make_unique<Encoding>();

	RemainingWork work(*enc);

	// add all seen possibilities -- this saves interference
	Cfg init00 = mk_init_cfg(prog, linobs);
	Cfg init01 = init00.copy();
	Cfg init10 = init00.copy();
	Cfg init11 = init00.copy();
	init01.seen[0] = true;
	init10.seen[1] = true;
	init11.seen[0] = true;
	init11.seen[1] = true;
	work.add(std::move(init00));
	work.add(std::move(init01));
	work.add(std::move(init10));
	work.add(std::move(init11));

	#if WORKLIST_INTERFERENCE
		/* SEQUENTIAL and INTERFERENCE steps intertwined using WORKLIST */

		std::size_t counter = 0;
		std::cerr << "Fixpoint Computation..." << std::endl;
		std::cerr << "\t[#steps\t\t#enc\t\t#interference]" << std::endl;

		while (!work.done()) {
			const Cfg& topost = work.pop();
			work.add(tmr::mk_all_post(topost, prog));
			mk_cfg_interference(*enc, work, topost);

			counter++;
			if (counter%500 == 0) {
				std::cerr << "\t[";
				std::cerr << counter/1000 << "." << (counter-((counter/1000)*1000))/100 << "k\t\t";
				std::cerr << enc->size()/1000 << "." << (enc->size()-((enc->size()/1000)*1000))/100 << "k\t\t";
				std::cerr << INTERFERENCE_STEPS/1000 << "k]";
				std::cerr << std::endl;
			}
		}

	#else
		/* independent SEQUENTIAL and INTERFERENCE steps */

		while (!work.done()) {
			std::size_t counter = 0;
			std::cerr << "post image...     [#enc]";

			// sequential steps
			while (!work.done()) {
				const Cfg& topost = work.pop();
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
			tmr::mk_all_interference(*enc, work);
		}

	#endif

	bool is_fp_sound = chk_aba_awareness(*enc);
	if (!is_fp_sound) std::cerr << "FIXPOINT SOLUTION UNSOUND!" << std::endl;

	return enc;
}
