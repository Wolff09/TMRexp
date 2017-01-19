#include "fixpoint.hpp"

#include <set>
#include <deque>
#include "fixp/cfgpost.hpp"
#include "fixp/interference.hpp"
#include "counter.hpp"
#include "config.hpp"
#include "chkmimic.hpp"

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
	
#if REPLACE_INTERFERENCE_WITH_SUMMARY && USE_MODIFIED_FIXEDPOINT
	static bool subset_shared(const Cfg& cc, const Cfg& sc) {
		// check observer state (ignoring free observers)
		for (std::size_t i = 0; i < cc.state.states().size()-2; i++)
			if (cc.state.states()[i] != sc.state.states()[i])
				return false;

		// global to global relations should be identical
		for (std::size_t i = cc.shape->offset_program_vars(); i < cc.shape->offset_locals(0); i++) {
			for (std::size_t j = i+1; j < cc.shape->offset_locals(0); j++) {
				if (!subset(cc.shape->at(i, j), sc.shape->at(i, j))) {				
					return false;
				}
			}
		}

		// global to special relations should be identical
		for (std::size_t i = 0; i < cc.shape->offset_vars(); i++) {
			for (std::size_t j = cc.shape->offset_program_vars(); j < cc.shape->offset_locals(0); j++) {
				if (!subset(cc.shape->at(i, j), sc.shape->at(i, j))) {
					return false;
				}
			}
		}

		// global to observer relations should be identical modulo local observer
		// that is: ignore relations that indicate oberserver is not reachable via shared
		for (std::size_t i = cc.shape->offset_vars(); i < cc.shape->offset_program_vars(); i++) {
			for (std::size_t j = cc.shape->offset_program_vars(); j < cc.shape->offset_locals(0); j++) {
				auto lhsc = cc.shape->at(i,j);
				auto rhsc = sc.shape->at(i,j);
				// if (lhsc == rhsc) continue;
				auto lhs = intersection(lhsc, EQ_MF_GF);
				auto rhs = intersection(rhsc, EQ_MF_GF);
				if (!subset(lhs, rhs)) {
					return false;
				}
			}
		}

		return true;
	}

	static bool ignore_for_summary(const Statement& stmt) {
		auto& fun = stmt.function();
		auto& prog = fun.prog();

		// is the init function about to be executed?
		if (&fun == &prog.init_fun()) {
			return true;
		}

		// is a summary about to be executed?
		if (prog.is_summary_statement(stmt)) {
			return true;
		}

		return false;
	}

	static std::deque<std::reference_wrapper<const Cfg>> find_effectful_configurations(const Cfg& precfg, std::vector<Cfg>& postcfgs) {
		std::deque<std::reference_wrapper<const Cfg>> result;
		for (const auto& cfg : postcfgs)
			if (!subset_shared(cfg, precfg))
				result.push_back(cfg);
		return result;
	}


	static void check_cfg(const Cfg& cfg) {
		if (cfg.pc[0] == NULL) {
			return;
		}

		auto post = tmr::post(cfg, 0);

		// initial and summaries do not need a summary
		auto& stmt = *cfg.pc[0];
		if (ignore_for_summary(stmt)) {
			return;
		}

		// find those post cfgs that require a summary, i.e. that changed the shared heap
		auto require_summaries = find_effectful_configurations(cfg, post);
		if (require_summaries.size() == 0) {
			return;
		}

		// frees shall have an empty summary
		if (stmt.clazz() == Statement::FREE) {
			// if a free comes that far, we are in trouble as it requires a non-empty summary
			throw std::runtime_error("Misbehaving Summary: free stmt requires non-empty summary.");
		}

		// prepare summary
		Cfg tmp = cfg.copy();
		tmp.pc[0] = &stmt.function().summary();
		if (stmt.function().has_output()) tmp.inout[0] = OValue();

		// execute summary
		auto sumpost = tmr::post(tmp, 0);

		// check summary
		for (const Cfg& postcfg : require_summaries) {
			bool covered = false;
			for (const Cfg& summarycfg : sumpost) {
				if (subset_shared(postcfg, summarycfg)) {
					covered = true;
					break;
				}
			}
			if (!covered) {
				// std::cout << std::endl << std::endl;
				// std::cout << cfg << *cfg.shape;
				// std::cout << "------------" << std::endl << std::endl;
				// std::cout << postcfg << *postcfg.shape;
				// std::cout << "------------" << std::endl << std::endl;
				// std::cout << "had " << sumpost.size() << " options" << std::endl;
				// for (const Cfg& summarycfg : sumpost) {
				// 	std::cout << std::endl << "[OPTION] " << summarycfg << *summarycfg.shape << std::endl;
				// }
				throw std::runtime_error("Misbehaving Summary: failed to mimic low-level action.");
			}
		}
	}
#endif

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
