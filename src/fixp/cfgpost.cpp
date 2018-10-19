#include "fixp/cfgpost.hpp"

#include <stdexcept>
#include "helpers.hpp"

using namespace tmr;


/******************************** FILTER HELPER ********************************/

inline bool filter_pc(Cfg& cfg, const unsigned short tid) {
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

inline void prune_noops(Cfg& cfg, unsigned short tid) {
	while (filter_pc(cfg, tid)) { /* empty */ }
}


/******************************** OVALUE HELPER ********************************/

inline std::vector<DataValue> get_possible_data_args(const Cfg& cfg, const Function& fun) {
	if (fun.has_arg()) return {{ DataValue::DATA, DataValue::OTHER }};
	else return { DEFAULT_DATA_VALUE };
}


/******************************** EVENT HELPER *********************************/

inline void fire_smr_event(Cfg& cfg, Event evt) {
	cfg.smrstate = cfg.smrstate.next(evt);
}

inline void fire_thread_event(Cfg& cfg, std::size_t index, Event evt) {
	switch (index) {
		case 0: cfg.threadstate[0] = cfg.threadstate[0].next(evt); break;
		case 1: cfg.threadstate[1] = cfg.threadstate[1].next(evt); break;
		default: throw std::logic_error("Unsupported threadstate index.");
	}
}

inline void fire_enter_event(Cfg& cfg, const Function& callee, unsigned short tid, DataValue dval) {
	fire_smr_event(cfg, Event::mk_enter(callee, cfg.offender[tid], dval));
	fire_thread_event(cfg, 0, Event::mk_enter(callee, tid == 0, dval));
	if (tid == 1) { // threadstate[1] only present when tid == 1
		fire_thread_event(cfg, 1, Event::mk_enter(callee, true, dval));
	}
}

inline void fire_exit_event(Cfg& cfg, unsigned short tid) {
	fire_smr_event(cfg, Event::mk_exit(cfg.offender[tid]));
	fire_thread_event(cfg, 0, Event::mk_exit(tid == 0));
	if (tid == 1) { // threadstate[1] only present when tid == 1
		fire_thread_event(cfg, 1, Event::mk_exit(true));
	}
}


/******************************** ENTER/EXIT HELPER *********************************/

inline void handle_exit(Cfg& cfg, unsigned short tid) {
	if (cfg.pc[tid] == NULL) {
		// currently called function exited
		fire_exit_event(cfg, tid);
		cfg.arg[tid] = DEFAULT_DATA_VALUE;
	}
}

inline void handle_enter(Cfg& cfg, const Function& callee, DataValue arg, unsigned short tid) {
	cfg.pc[tid] = &callee.body();
	cfg.arg[tid] = arg;
	fire_enter_event(cfg, callee, tid, arg);
	// prune_noops(cfg, tid); // this may hide exit events
}

inline bool drop_enter_cfg(const Cfg& cfg, unsigned short tid) {
	//	if (cf.state.states().at(0)->name() == "base:double-retire") {
	//		return true
	//	}
	// return false;
	return cfg.smrstate.is_marked() || cfg.threadstate[0].is_marked() || cfg.threadstate[1].is_marked();
}


/******************************** POST FOR ONE THREAD ********************************/

#define DEBUG_POST_WITH_SHAPE false
#define DEBUG_POST_WITHOUT_SHAPE false

inline void debug_post_input(const Cfg& cfg) {
	#if DEBUG_POST_WITH_SHAPE || DEBUG_POST_WITHOUT_SHAPE
		std::cout << std::endl << std::endl;
		std::cout << "==============================================================" << std::endl;
		std::cout << "posting: " << cfg;
		#if DEBUG_POST_WITH_SHAPE
			std::cout << *cfg.shape;
		#endif
		std::cout << std::endl;
	#endif
}

inline void debug_post_output(const Cfg& cfg) {
	#if DEBUG_POST_WITH_SHAPE || DEBUG_POST_WITHOUT_SHAPE
		std::cout << "adding: " << cfg << std::endl;
		#if DEBUG_POST_WITH_SHAPE
			std::cout << *cfg.shape;
		#endif
		std::cout << std::endl;
	#endif
}

void mk_tid_post(std::vector<Cfg>& result, const Cfg& input, unsigned short tid, const Program& prog) {
	debug_post_input(input);

	if (input.pc[tid] != NULL) {
		// make post image
		std::vector<Cfg> postcfgs = tmr::post(input, tid);

		// post process post images
		for (Cfg& pcf : postcfgs) {
			prune_noops(pcf, tid);
			handle_exit(pcf, tid);

			debug_post_output(pcf);
		}

		// add to result
		result.reserve(result.size() + postcfgs.size());
		std::move(postcfgs.begin(), postcfgs.end(), std::back_inserter(result));

	} else {
		// invoke function
		result.reserve(result.size() + 2*prog.size());
		for (std::size_t i = 0; i < prog.size(); i++) {
			const Function& fun = prog.at(i);
			auto dvals = get_possible_data_args(input, fun);
			for (DataValue arg : dvals) {
				result.push_back(input.copy());
				handle_enter(result.back(), fun, arg, tid);
				if (drop_enter_cfg(result.back(), tid)) {
					result.pop_back();
				}

				debug_post_output(result.back());
			}
		}
	}
}


/******************************** POST FOR ALL THREADS ********************************/

std::vector<Cfg> tmr::mk_all_post(const Cfg& cfg, unsigned short tid, const Program& prog) {
	std::vector<Cfg> result;
	mk_tid_post(result, cfg, tid, prog);
	return result;
}

std::vector<Cfg> tmr::mk_all_post(const Cfg& cfg, const Program& prog) {
	return mk_all_post(cfg, 0, prog);
}
