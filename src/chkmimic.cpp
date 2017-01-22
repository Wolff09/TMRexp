#include "chkmimic.hpp"

#include <deque>
#include "post.hpp"
#include "config.hpp"
#include "counter.hpp"

using namespace tmr;


static bool subset_shared(const Cfg& cc, const Cfg& sc) {
	auto pred = [](RelSet lhs, RelSet rhs) -> bool {
		return subset(lhs, rhs);
	};

	// check observer state (ignoring free observers)
	for (std::size_t i = 0; i < cc.state.states().size()-2; i++)
		if (cc.state.states()[i] != sc.state.states()[i])
			return false;

	// global to global relations should be identical
	for (std::size_t i = cc.shape->offset_program_vars(); i < cc.shape->offset_locals(0); i++) {
		for (std::size_t j = i+1; j < cc.shape->offset_locals(0); j++) {
			if (!pred(cc.shape->at(i, j), sc.shape->at(i, j))) {				
				return false;
			}
		}
	}

	// global to special relations should be identical
	for (std::size_t i = 0; i < cc.shape->offset_vars(); i++) {
		for (std::size_t j = cc.shape->offset_program_vars(); j < cc.shape->offset_locals(0); j++) {
			if (!pred(cc.shape->at(i, j), sc.shape->at(i, j))) {
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
			if (!pred(lhs, rhs)) {
				return false;
			}
		}
	}

	return true;
}

static std::deque<std::reference_wrapper<const Cfg>> find_effectful_configurations(const Cfg& precfg, std::vector<Cfg>& postcfgs) {
	std::deque<std::reference_wrapper<const Cfg>> result;
	for (const auto& cfg : postcfgs)
		if (!subset_shared(cfg, precfg))
			result.push_back(cfg);
	return result;
}

static bool check_disambiguated_cfg(const Cfg& cfg) {
	if (cfg.pc[0] == NULL) {
		return true;
	}

	auto post = tmr::post(cfg, 0, PRF);
	auto& stmt = *cfg.pc[0];

	// find those post cfgs that require a summary, i.e. that changed the shared heap
	auto require_summaries = find_effectful_configurations(cfg, post);
	if (require_summaries.size() == 0) {
		return true;
	}
	
	SUMMARIES_NEEDED++;

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
	auto sumpost = tmr::post(tmp, 0, PRF);

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
			std::cout << std::endl << std::endl;
			std::cout << cfg << *cfg.shape;
			std::cout << "------------" << std::endl << std::endl;
			std::cout << postcfg << *postcfg.shape;
			std::cout << "------------" << std::endl << std::endl;
			std::cout << "had " << sumpost.size() << " options" << std::endl;
			for (const Cfg& summarycfg : sumpost) {
				std::cout << std::endl << "[OPTION] " << summarycfg << *summarycfg.shape << std::endl;
			}
			return false;
		}
	}

	return true;
}

static bool check_cfg(const Cfg& cfg, std::size_t row) {
	if (cfg.pc[0] == NULL) {
		return true;
	}

	if (row < cfg.shape->offset_locals(0)) {

		std::vector<Shape*> dis = disambiguate(*cfg.shape, row);
		for (Shape* s : dis) {
			Cfg tmp(cfg, std::move(s));
			if (!check_cfg(tmp, row+1)) {
				return false;
			}
		}
		return true;

	} else {
		return check_disambiguated_cfg(cfg);
	}
}

static bool check_cfg(const Cfg& cfg) {
	if (cfg.pc[0] == NULL) {
		return true;
	}

	if (cfg.pc[0]->function().prog().precise_check_mimick()) {
		return check_cfg(cfg, cfg.shape->offset_program_vars());
	} else {
		return check_disambiguated_cfg(cfg);	
	}
}

bool tmr::chk_mimic(const Encoding& fixedpoint, MemorySetup msetup) {
	#if !REPLACE_INTERFERENCE_WITH_SUMMARY
		throw std::runtime_error("CHK-MIMIC available only in summary mode.");
	#endif

	if (msetup != PRF) {
		throw std::runtime_error("CHK-MIMIC available for PRF semantics only.");
	}

	for (auto& region : fixedpoint) {
		for (auto it = region.second.begin(); it != region.second.end(); it++) {
			if (!check_cfg(*it)) {
				return false;
			}
		}
	}

	return true;
}
