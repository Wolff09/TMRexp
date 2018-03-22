#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/setout.hpp"
#include "config.hpp"

using namespace tmr;


static inline std::vector<Shape*> split_shape(const Cfg& cfg, unsigned short tid) {
	// split shape such that no relation of two local variables for thread tid contain = and !=

	std::vector<Shape*> result;
	std::vector<Shape*> worklist;
	worklist.reserve(cfg.shape->sizeLocals()*2);
	worklist.push_back(new Shape(*cfg.shape));

	while (!worklist.empty()) {
		Shape* s = worklist.back();
		if (s == NULL) continue;
		worklist.pop_back();
		bool is_split = true;

		for (std::size_t i = s->offset_locals(tid); i < s->offset_locals(tid)+s->sizeLocals(); i++) {
			for (std::size_t j = i+1; j < s->offset_locals(tid)+s->sizeLocals(); j++) {
				if (s->test(i, j, EQ) && intersection(s->at(i, j), MT_GT_MF_GF_BT).any()) {
					worklist.push_back(isolate_partial_concretisation(*s, i, j, EQ_));
					worklist.push_back(isolate_partial_concretisation(*s, i, j, MT_GT_MF_GF_BT));
					is_split = false;
					break;
				}
			}
		}

		if (is_split) {
			result.push_back(s);
		} else {
			delete s;
		}
	}

	return result;
}

static inline void fire_event(const Cfg& cfg, DynamicSMRState& state, const Function& eqevt, const Function& neqevt, std::size_t var, unsigned short tid) {
	const Shape& shape = *cfg.shape;
	for (std::size_t i = shape.offset_locals(tid); i < shape.offset_locals(tid)+shape.sizeLocals(); i++) {
		auto& evt = shape.test(var, i, EQ) ? eqevt : neqevt;
		if (state.at(i)) {
			state.set(i, &state.at(i)->next(evt, OValue::Anonymous()));
		}
		if (cfg.own.at(i) && shape.test(var, i, EQ)) {
			throw std::logic_error("Owned cells must not be guarded/retired.");
		}
	}
}

static inline std::vector<Cfg> smrpost(const Cfg& cfg, const Function& eqevt, const Function& neqevt, std::size_t var, unsigned short tid, bool fire0, bool fire1) {
	auto shapes = split_shape(cfg, tid);
	std::vector<Cfg> result;
	for (Shape* shape : shapes) {
		result.push_back(mk_next_config(cfg, shape, tid));
		if (fire0) fire_event(cfg, cfg.guard0state, eqevt, neqevt, var, tid);
		if (fire1) fire_event(cfg, cfg.guard1state, eqevt, neqevt, var, tid);
	}

	return result;
}

/******************************** RETIRE ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const Retire& stmt, unsigned short tid) {
	CHECK_STMT;
	auto& evt = stmt.function().prog().retirefun();
	auto var = mk_var_index(*cfg.shape, stmt.decl(), tid);
	if (!cfg.valid_ptr.at(var)) raise_rpr(cfg, var, "Call to retire with invalid pointer.");
	return smrpost(cfg, evt, evt, var, tid, true, true);
}


/******************************** GUARD ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const HPset& stmt, unsigned short tid) {
	CHECK_STMT;
	auto& eqevt = stmt.function().prog().guardfun();
	auto& neqevt = stmt.function().prog().unguardfun();
	auto var = mk_var_index(*cfg.shape, stmt.decl(), tid);
	return smrpost(cfg, eqevt, neqevt, var, tid, stmt.hpindex() == 0, stmt.hpindex() == 1);
}
