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

		// the CAS stmt appears either in an ITE or on its own (in the latter case, the next field is set)
		assert(tmp.pc[tid]->next() == NULL || (tmp.pc[tid]->next() == nextTrue && tmp.pc[tid]->next() == nextFalse));

		// execute: dst = src (beware, the following advances the pc)
		auto tmppost = tmr::post_assignment_pointer(tmp, stmt.dst(), stmt.src(), tid, &stmt);

		for (Cfg& cf : tmppost) {
			// if CAS appears on its own, we don't want the pc to continue, otherwise it is NULL anyway
			cf.pc[tid] = NULL;

			// lp post, or noop
			result.push_back(std::move(cf));
		}

		// set proper pc
		for (Cfg& rescfg : result) {
			rescfg.pc[tid] = nextTrue;
		}
	}

	if (sp.second != NULL) {
		// compare evaluates to false => just do nothing and go on
		result.push_back(mk_next_config(cfg, sp.second, nextFalse, tid));
	}

	return result;
}
