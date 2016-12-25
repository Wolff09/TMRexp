#include "post.hpp"

#include <assert.h>
#include <deque>
#include "helpers.hpp"
#include "config.hpp"

using namespace tmr;



static std::vector<Cfg> get_post_cfgs(const Cfg& cfg, unsigned short tid) {
	const Statement& stmt = *cfg.pc[tid];
	switch (stmt.clazz()) {
		case Statement::SQZ:     return tmr::post(cfg, static_cast<const              Sequence&>(stmt), tid);
		case Statement::ATOMIC:  return tmr::post(cfg, static_cast<const                Atomic&>(stmt), tid);
		case Statement::CAS:     return tmr::post(cfg, static_cast<const        CompareAndSwap&>(stmt), tid);
		case Statement::ASSIGN:  return tmr::post(cfg, static_cast<const            Assignment&>(stmt), tid);
		case Statement::SETNULL: return tmr::post(cfg, static_cast<const        NullAssignment&>(stmt), tid);
		case Statement::INPUT:   return tmr::post(cfg, static_cast<const   ReadInputAssignment&>(stmt), tid);
		case Statement::OUTPUT:  return tmr::post(cfg, static_cast<const WriteOutputAssignment&>(stmt), tid);
		case Statement::MALLOC:  return tmr::post(cfg, static_cast<const                Malloc&>(stmt), tid);
		case Statement::FREE:    return tmr::post(cfg, static_cast<const                  Free&>(stmt), tid);
		case Statement::BREAK:   return tmr::post(cfg, static_cast<const                 Break&>(stmt), tid);
		case Statement::LINP:    return tmr::post(cfg, static_cast<const    LinearizationPoint&>(stmt), tid);
		case Statement::ITE:     return tmr::post(cfg, static_cast<const                   Ite&>(stmt), tid);
		case Statement::WHILE:   return tmr::post(cfg, static_cast<const                 While&>(stmt), tid);
		case Statement::ORACLE:  return tmr::post(cfg, static_cast<const                Oracle&>(stmt), tid);
		case Statement::CHECKP:  return tmr::post(cfg, static_cast<const         CheckProphecy&>(stmt), tid);
		case Statement::KILL:    return tmr::post(cfg, static_cast<const                Killer&>(stmt), tid);
	}
	assert(false);
}

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

bool ignore_for_summary(const Statement& stmt) {
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

std::deque<std::reference_wrapper<const Cfg>> find_effectful_configurations(const Cfg& precfg, std::vector<Cfg>& postcfgs) {
	std::deque<std::reference_wrapper<const Cfg>> result;
	for (const auto& cfg : postcfgs)
		if (!subset_shared(cfg, precfg))
			result.push_back(cfg);
	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, unsigned short tid) {
	// execute low-level action
	auto post = get_post_cfgs(cfg, tid);

	#define RETURN return post;

	// // check for high-level simulation
	// #if REPLACE_INTERFERENCE_WITH_SUMMARY
	// 	assert(tid == 0);

	// 	// initial and summaries do not need a summary
	// 	auto& stmt = *cfg.pc[tid];
	// 	if (ignore_for_summary(stmt)) {
	// 		RETURN;
	// 	}

	// 	// find those post cfgs that require a summary, i.e. that changed the shared heap
	// 	auto require_summaries = find_effectful_configurations(cfg, post);
	// 	if (require_summaries.size() == 0) {
	// 		RETURN;
	// 	}

	// 	// frees shall have an empty summary
	// 	if (stmt.clazz() == Statement::FREE) {
	// 		// if a free comes that far, we are in trouble as it requires a non-empty summary
	// 		throw std::runtime_error("Misbehaving Summary: free stmt requires non-empty summary.");
	// 	}

	// 	// prepare summary
	// 	Cfg tmp = cfg.copy();
	// 	tmp.pc[tid] = &stmt.function().summary();
	// 	if (stmt.function().has_output()) tmp.inout[tid] = OValue();

	// 	// execute summary
	// 	auto sumpost = get_post_cfgs(tmp, tid);

	// 	// check summary
	// 	for (const Cfg& postcfg : require_summaries) {
	// 		bool covered = false;
	// 		for (const Cfg& summarycfg : sumpost) {
	// 			if (subset_shared(postcfg, summarycfg)) {
	// 				covered = true;
	// 				break;
	// 			}
	// 		}
	// 		if (!covered) throw std::runtime_error("Misbehaving Summary: failed to mimic low-level action.");
	// 	}
	// #endif

	RETURN;
}
