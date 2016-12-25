#include "post/eval.hpp"

#include "config.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/assign.hpp"
#include <deque>
#include <stack>

using namespace tmr;



std::vector<Cfg> tmr::eval_cond_cas(const Cfg& cfg, const CompareAndSwap& stmt, const Statement* nextTrue, const Statement* nextFalse, unsigned short tid) {
	std::vector<Cfg> result;

	// check the shape for condition
	std::pair<Shape*, Shape*> sp = eval_eqneq(cfg, stmt.dst(), stmt.cmp(), false, tid);

	if (sp.first != NULL) {
		// compare evaluates to true
		Cfg tmp(cfg, sp.first);

		// execute: dst = src (beware, the following advances the pc and updates age fields)
		tmp = tmr::post_assignment_pointer(tmp, stmt.dst(), stmt.src(), tid, &stmt);
		tmp.pc[tid] = NULL; // if CAS appears on its own, we don't want the pc to continue, otherwise it is NULL anyway

		// execute: linearization point (if needed)
		if (stmt.fires_lp()) {
			tmp.pc[tid] = &stmt.lp();
			result = tmr::post(tmp, stmt.lp(), tid);
		} else {
			result.push_back(std::move(tmp));
		}

		// update ages
		std::deque<Cfg> tmpres;
		for (Cfg& precfg : result) {
			assert(precfg.pc[tid] == NULL);
			precfg.pc[tid] = nextTrue;
		}

		result.reserve(result.size() + tmpres.size());
		for (Cfg& mv : tmpres) result.push_back(std::move(mv));
	}

	if (sp.second != NULL) {
		#if REPLACE_INTERFERENCE_WITH_SUMMARY
			if (stmt.function().prog().is_summary_statement(stmt))
				throw std::runtime_error("Failing CAS in summary not supported.");
		#endif

		// compare evaluates to false => just do nothing and go on
		result.push_back(mk_next_config(cfg, sp.second, nextFalse, tid));
	}

	assert(result.size() > 0);
	return result;
}
