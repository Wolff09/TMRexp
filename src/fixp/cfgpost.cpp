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


/******************************** LOCAL VAR HELPER ********************************/

bool are_locals_undef(const Shape& shape, const unsigned short tid) {
	for (std::size_t i = shape.offset_locals(tid); i < shape.sizeLocals(); i++)
		for (std::size_t t = 0; t < shape.size(); t++)
			if (t == shape.index_UNDEF() && shape.at(i, t) != MT_) return false;
			else if (t == i && shape.at(t, t) != EQ_) return false;
			else if (shape.at(t, i) != BT_) return false;
	return true;
}

void set_locals_undef(Cfg& cfg, const unsigned short tid) {
	Shape& shape = *cfg.shape;

	for (std::size_t i = shape.offset_locals(tid); i < shape.offset_locals(tid) + shape.sizeLocals(); i++) {
		for (std::size_t t = 0; t < shape.size(); t++) {
			shape.set(i, t, BT);
		}
		shape.set(i, i, EQ);
		shape.set(i, shape.index_UNDEF(), MT);
	}
}


/******************************** OVALUE HELPER ********************************/

std::vector<OValue> get_possible_ovaluess(const Cfg& cfg, const Observer& observer, const Function& fun) {
	assert(fun.has_input() ^ fun.has_output());
	std::vector<OValue> result;
	if (fun.has_input()) {
		for (std::size_t i = 0; i < observer.numVars(); i++)
			if (!cfg.seen[i])
				result.push_back(observer.mk_var(i));
		result.push_back(OValue::Anonymous());
	} else {
		result.push_back(OValue());
	}
	return result;
}


/******************************** POST FOR ONE THREAD ********************************/

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
				if (pcf.inout[tid].type() == OValue::DUMMY)
					if (input.pc[tid]->function().has_output())
						throw std::runtime_error("Return value is missing or was read from a potentially free cell.");
				// if the currently called function returned, all local info is reset (goes out of scope)
				set_locals_undef(pcf, tid);
				pcf.inout[tid] = OValue();
				pcf.oracle[tid] = false;
				for (std::size_t i = 0; i < pcf.shape->sizeLocals(); i++) {
					pcf.own.set(pcf.shape->offset_locals(tid) + i, true);
					pcf.valid_ptr.set(pcf.shape->offset_locals(tid) + i, false);
					pcf.valid_next.set(pcf.shape->offset_locals(tid) + i, false);
					pcf.guard0state.set(pcf.shape->offset_locals(tid) + i, nullptr);
					pcf.guard1state.set(pcf.shape->offset_locals(tid) + i, nullptr);
				}
			}
			result.push_back(std::move(pcf));
		}
	} else {
		// when entering a method all local variables of the executing thread should point to UNDEF
		// if the called method has input, the input is chosen, if it has output, the output is initialized empty
		const Observer& observer = input.state.observer();
		// result.reserve(result.size() + prog.size());
		for (std::size_t i = 0; i < prog.size(); i++) {
			const Function& fun = prog.at(i);
			std::vector<OValue> ovals = get_possible_ovaluess(input, observer, fun);
			for (OValue ov : ovals) {
				result.push_back(input.copy());
				result.back().pc[tid] = &fun.body();
				result.back().inout[tid] = ov;
				if (ov.type() == OValue::OBSERVABLE)
					result.back().seen[ov.id()] = true;
				// filter out noops
				while (filter_pc(result.back(), tid)) { /* empty */ }
			}
		}
	}
}


/******************************** POST FOR ALL THREADS ********************************/

std::vector<Cfg> tmr::mk_all_post(const Cfg& cfg, const Program& prog) {
	std::vector<Cfg> result = post_free(cfg, 0, prog);
	mk_tid_post(result, cfg, 0, prog);
	return result;
}
