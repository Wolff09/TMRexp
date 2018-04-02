#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include <deque>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/setout.hpp"
#include "config.hpp"

using namespace tmr;


static inline std::deque<Shape*> split_shape(const Cfg& cfg, std::size_t begin, std::size_t end, bool splitReuse=false) {
	std::deque<Shape*> result;
	result.push_back(new Shape(*cfg.shape));
	for (std::size_t i = splitReuse ? cfg.shape->index_REUSE() : begin; i < end; i = i < begin ? begin : i+1) {
		for (std::size_t j = i+1; j < end; j++) {
			std::size_t size = result.size();
			for (std::size_t k = 0; k < size; k++) {
				Shape* shape = result.at(k);
				if (!shape) continue;
				Shape* eq_shape = isolate_partial_concretisation(*shape, i, j, EQ_);
				Shape* neq_shape = isolate_partial_concretisation(*shape, i, j, MT_GT_MF_GF_BT);
				result[k] = eq_shape ? eq_shape : neq_shape;
				if (eq_shape && neq_shape) result.push_back(neq_shape);
				delete shape;
			}
		}
	}
	// erase NULL
	for (auto it = result.begin(); it != result.end(); ) {
        if (*it == NULL) it = result.erase(it);
        else ++it;
    }
    return result;
}

static inline void fire_event(const Cfg& cfg, DynamicSMRState& state, const Function* eqevt, const Function* neqevt, std::size_t var, std::size_t begin, std::size_t end) {
	const Shape& shape = *cfg.shape;
	for (std::size_t i = begin; i < end; i++) {
		auto& evt = shape.test(var, i, EQ) ? eqevt : neqevt;
		if (!evt) continue;
		if (state.at(i)) {
			state.set(i, &state.at(i)->next(*evt, OValue::Anonymous()));
		}
		if (cfg.own.at(i) && shape.test(var, i, EQ)) {
			throw std::runtime_error("Owned cells must not be guarded/retired.");
		} else if (evt->name() == "retire" && i < cfg.shape->offset_locals(0)) {
			throw std::runtime_error("Invariant violation: shared cells must not be retired.");
		}
	}
}

static inline std::vector<Cfg> smrpost(const Cfg& cfg, const Function* eqevt, const Function* neqevt, std::size_t var, bool fire0, bool fire1, unsigned short tid, std::size_t begin, std::size_t end) {
	auto shapes = split_shape(cfg, begin, end);
	std::vector<Cfg> result;
	for (Shape* shape : shapes) {
		result.push_back(mk_next_config(cfg, shape, tid));
		if (fire0) fire_event(result.back(), result.back().guard0state, eqevt, neqevt, var, begin, end);
		if (fire1) fire_event(result.back(), result.back().guard1state, eqevt, neqevt, var, begin, end);
	}

	return result;
}

static inline std::vector<Cfg> smrpost(const Cfg& cfg, const Function* eqevt, const Function* neqevt, std::size_t var, bool fire0, bool fire1, unsigned short tid) {
	const Shape& shape = *cfg.shape;
	std::size_t begin = shape.offset_locals(tid);
	std::size_t end = shape.offset_locals(tid)+shape.sizeLocals();
	return smrpost(cfg, eqevt, neqevt, var, fire0, fire1, tid, begin, end);
}

/******************************** RETIRE ********************************/

static bool is_shared_reachable(const Shape& shape, std::size_t var) {
	for (std::size_t i = shape.offset_program_vars(); i < shape.offset_locals(0); i++) {
		if (intersection(shape.at(i, var), EQ_MT_GT).any())
			return true;
	}
	return false;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const Retire& stmt, unsigned short tid) {
	CHECK_STMT;
	auto& evt = stmt.function().prog().retirefun(); // fire no event if guard is for another address
	auto var = mk_var_index(*cfg.shape, stmt.decl(), tid);

	if (!cfg.valid_ptr.at(var)) raise_rpr(cfg, var, "Call to retire with invalid pointer.");
	if (cfg.own.at(var)) raise_epr(cfg, var, "Owned addresses must not be retired.");
	if (var < cfg.shape->offset_locals(tid)) throw std::logic_error("Retire must not use non-local pointers.");
	// check for retire of shared done below (more precise)

	std::size_t begin = cfg.shape->offset_program_vars(); // cfg.shape->offset_locals(0);
	std::size_t end = cfg.shape->offset_locals(tid)+cfg.shape->sizeLocals();
	
	// return smrpost(cfg, &evt, nullptr, var, true, true, tid, begin, end);
	auto shapes = split_shape(cfg, begin, end, true);
	std::vector<Cfg> result;
	for (Shape* shape : shapes) {
		result.push_back(mk_next_config(cfg, shape, tid));
		Cfg& cf = result.back();
		fire_event(cf, cf.guard0state, &evt, nullptr, var, begin, end);
		fire_event(cf, cf.guard1state, &evt, nullptr, var, begin, end);
		if (shape->test(var, shape->index_REUSE(), EQ)) {
			bool is_retired = cf.retired;
			bool is_observed = cf.shape->test(var, shape->index_ObserverVar(0), EQ) || cf.shape->test(var, shape->index_ObserverVar(1), EQ);
			bool is_observed_committed = cf.inout[tid].type() == OValue::OBSERVABLE;
			if (is_retired && is_observed && is_observed_committed) raise_epr(cfg, var, "Double retire on REUSE+observed address.");
			cf.retired = true;
		}
		if (is_shared_reachable(*shape, var)) {
			std::cout << "Retire of shared reachable address." << std::endl;
			std::cout << cfg << *cfg.shape << std::endl;
			std::cout << "In split shape: " << std::endl << *shape << std::endl;
			throw std::runtime_error("Retire of shared reachable address.");
		}
	}
	return result;
}


/******************************** GUARD ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const HPset& stmt, unsigned short tid) {
	CHECK_STMT;
	auto& eqevt = stmt.function().prog().guardfun();
	auto& neqevt = stmt.function().prog().unguardfun();
	auto var = mk_var_index(*cfg.shape, stmt.decl(), tid);
	return smrpost(cfg, &eqevt, &neqevt, var, stmt.hpindex() == 0, stmt.hpindex() == 1, tid);
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
