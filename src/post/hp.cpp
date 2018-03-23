#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/setout.hpp"
#include "config.hpp"

using namespace tmr;


static inline std::vector<Shape*> split_shape(const Cfg& cfg, std::size_t begin, std::size_t end) {
	// split shape such that no relation of two local variables for thread tid contain = and !=

	std::vector<Shape*> result;
	std::vector<Shape*> worklist;
	worklist.reserve((end - begin)*2);
	worklist.push_back(new Shape(*cfg.shape));

	while (!worklist.empty()) {
		Shape* s = worklist.back();
		worklist.pop_back();
		if (s == NULL) continue;
		bool is_split = true;

		for (std::size_t i = begin; i < end; i++) {
			for (std::size_t j = i+1; j < end; j++) {
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

static inline void fire_event(const Cfg& cfg, DynamicSMRState& state, const Function& eqevt, const Function& neqevt, std::size_t var, std::size_t begin, std::size_t end) {
	const Shape& shape = *cfg.shape;
	for (std::size_t i = begin; i < end; i++) {
		auto& evt = shape.test(var, i, EQ) ? eqevt : neqevt;
		if (state.at(i)) {
			state.set(i, &state.at(i)->next(evt, OValue::Anonymous()));
		}
		if (cfg.own.at(i) && shape.test(var, i, EQ)) {
			throw std::logic_error("Owned cells must not be guarded/retired.");
		}
	}
}

static inline std::vector<Cfg> smrpost(const Cfg& cfg, const Function& eqevt, const Function& neqevt, std::size_t var, bool fire0, bool fire1, unsigned short tid, std::size_t begin, std::size_t end) {
	auto shapes = split_shape(cfg, begin, end);
	std::vector<Cfg> result;
	for (Shape* shape : shapes) {
		result.push_back(mk_next_config(cfg, shape, tid));
		if (fire0) fire_event(cfg, result.back().guard0state, eqevt, neqevt, var, begin, end);
		if (fire1) fire_event(cfg, result.back().guard1state, eqevt, neqevt, var, begin, end);
	}

	return result;
}

static inline std::vector<Cfg> smrpost(const Cfg& cfg, const Function& eqevt, const Function& neqevt, std::size_t var, bool fire0, bool fire1, unsigned short tid) {
	const Shape& shape = *cfg.shape;
	std::size_t begin = shape.offset_locals(tid);
	std::size_t end = shape.offset_locals(tid)+shape.sizeLocals();
	return smrpost(cfg, eqevt, neqevt, var, fire0, fire1, tid, begin, end);
}

/******************************** RETIRE ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const Retire& stmt, unsigned short tid) {
	CHECK_STMT;
	auto& evt = stmt.function().prog().retirefun();
	auto var = mk_var_index(*cfg.shape, stmt.decl(), tid);

	if (!cfg.valid_ptr.at(var)) raise_rpr(cfg, var, "Call to retire with invalid pointer.");
	if (cfg.own.at(var)) raise_epr(cfg, var, "Owned addresses must not be retired.");
	if (var < cfg.shape->offset_locals(tid)) throw std::logic_error("Retire must not use non-local pointers.");

	std::size_t begin = cfg.shape->offset_locals(0);
	std::size_t end = cfg.shape->offset_locals(tid)+cfg.shape->sizeLocals();
	
	return smrpost(cfg, evt, evt, var, true, true, tid, begin, end);
}


/******************************** GUARD ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const HPset& stmt, unsigned short tid) {
	CHECK_STMT;
	auto& eqevt = stmt.function().prog().guardfun();
	auto& neqevt = stmt.function().prog().unguardfun();
	auto var = mk_var_index(*cfg.shape, stmt.decl(), tid);
	return smrpost(cfg, eqevt, neqevt, var, stmt.hpindex() == 0, stmt.hpindex() == 1, tid);
}


/******************************** UNGUARD ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const HPrelease& stmt, unsigned short tid) {
	CHECK_STMT;
	auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	auto& cf = result.back();
	auto& evt = stmt.function().prog().unguardfun();
	DynamicSMRState& state = stmt.hpindex() == 0 ? cf.guard0state : cf.guard1state;
	for (std::size_t i = cfg.shape->offset_locals(tid); i < cfg.shape->offset_locals(tid) + cfg.shape->sizeLocals(); i++) {
		if (state.at(i)) {
			state.set(i, &state.at(i)->next(evt, OValue::Anonymous()));
		}
	}
	return result;
}
