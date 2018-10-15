#include "fixp/cfgpost.hpp"

#include <stdexcept>
#include "helpers.hpp"

using namespace tmr;


/******************************** FILTER HELPER ********************************/

inline bool filter_pc(Cfg& cfg, const unsigned short& tid) {
	/* Some Statements do not modify the cfg => the post-image just copies the current cfg
	 * Filter them out after the post image
	 * 
	 * Advances cfg.pc[tid] if it is a noop; return true if pc was advanced
	 */
	const Statement* stmt = cfg.pc[tid];
	if (cfg.pc[tid] == NULL) return false;
	if (stmt->clazz() == Statement::SQZ || stmt->clazz() == Statement::BREAK) {
		cfg.pc[tid] = stmt->next();
		return true;
	}
	if (stmt->is_conditional()) {
		const Conditional* c = static_cast<const Conditional*>(stmt);
		if (c->cond().type() == Condition::TRUEC) {
			cfg.pc[tid] = static_cast<const Conditional*>(stmt)->next_true_branch();
			return true;
		}
	}
	return false;
}


/******************************** OVALUE HELPER ********************************/

inline std::vector<DataValue> get_possible_data_args(const Cfg& cfg, const Function& fun) {
	return {{ DataValue::DATA, DataValue::OTHER }};
}


/******************************** EVENT HELPER *********************************/

inline void fire_event(Cfg& cfg, Event evt) {
	cfg.state0 = cfg.state0.next(evt);
	cfg.state1 = cfg.state1.next(evt);
}

inline void fire_enter_event(Cfg& cfg, const Function& callee, unsigned short tid, DataValue dval) {
	auto event = Event::mk_enter(callee, tid, dval);
	fire_event(cfg, event);
}

inline void fire_exit_event(Cfg& cfg, unsigned short tid) {
	auto event = Event::mk_exit(tid);
	fire_event(cfg, event);
}


/******************************** POST FOR ONE THREAD ********************************/

// TODO: locals must not be undefified!!!!!

void mk_tid_post(std::vector<Cfg>& result, const Cfg& input, unsigned short tid, const Program& prog) {
	if (input.pc[tid] != NULL) {
		std::vector<Cfg> postcfgs = tmr::post(input, tid);
		result.reserve(result.size() + postcfgs.size());
		for (Cfg& pcf : postcfgs) {
			// if the pcf.pc[tid] is at an statement that is a noop (post image just copies the cfg), filter it out
			// to do so we advance the pc to next non-noop statement
			// this must be done before handling returning fuctions as the pc might be set to NULL
			while (filter_pc(pcf, tid)) { /* empty */ }
			if (pcf.pc[tid] == NULL) {
				// currently called function returned
				fire_exit_event(pcf, tid); // TODO: correct?
				pcf.arg[tid] = DataValue::OTHER;
			}
			result.push_back(std::move(pcf));
		}
	} else {
		// invoke function
		for (std::size_t i = 0; i < prog.size(); i++) {
			const Function& fun = prog.at(i);
			auto dvals = get_possible_data_args(input, fun);
			for (DataValue arg : dvals) {
				result.push_back(input.copy());
				Cfg& cf = result.back();
				cf.pc[tid] = &fun.body();
				cf.arg[tid] = arg;
				fire_enter_event(cf, fun, tid, arg); // TODO: correct?
				while (filter_pc(cf, tid)) { /* empty */ }
			}
		}
	}
}


/******************************** POST FOR ALL THREADS ********************************/

std::vector<Cfg> tmr::mk_all_post(const Cfg& cfg, const Program& prog) {
	std::vector<Cfg> result;
	mk_tid_post(result, cfg, 0, prog);
	mk_tid_post(result, cfg, 1, prog);
	return result;
}
