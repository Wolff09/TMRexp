#include "post/assign.hpp"
#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include <deque>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"

using namespace tmr;


std::vector<Cfg> tmr::post(const Cfg& cfg, const Assignment& stmt, unsigned short tid) {
	CHECK_STMT;

	const Expr& lhs = stmt.lhs();
	const Expr& rhs = stmt.rhs();

	auto result = stmt.lhs().type() == DATA ? post_assignment_data(   cfg, lhs, rhs, tid, &stmt)
	                                        : post_assignment_pointer(cfg, lhs, rhs, tid, &stmt);

	if (stmt.fires_lp()) {
		// pc in res was already advanced, it is the linearization point
		assert(res.pc[tid] == &stmt.lp());

		std::vector<Cfg> tmp;
		tmp.swap(result);
		result.reserve(tmp.size()*2);

		// post for linearization point
		for (const Cfg& cf : tmp) {
			auto linpres = tmr::post(cf, stmt.lp(), tid);
			std::move(linpres.begin(), linpres.end(), std::back_inserter(result));
			// result.insert(result.end(), linpres.begin(), linpres.end());
		}
	}

	return result;
}


/******************************** DATA: LHS.DATA = RHS.DATA ********************************/

std::vector<Cfg> tmr::post_assignment_data(const Cfg& cfg, const Expr& lhs, const Expr& rhs, unsigned short tid, const Statement* stmt) {
	assert(false);
	throw std::logic_error("Malicious call to tmr::post_assignment_data()");
}


/******************************** POINTER ********************************/


std::vector<Cfg> tmr::post_assignment_pointer(const Cfg& cfg, const Expr& lhs, const Expr& rhs, unsigned short tid, const Statement* stmt) {
	assert(lhs.type() == POINTER && rhs.type() == POINTER);
	const Shape& input = *cfg.shape;
	
	assert(lhs.clazz() == Expr::VAR || lhs.clazz() == Expr::SEL);
	assert(rhs.clazz() == Expr::VAR || rhs.clazz() == Expr::SEL);
	auto sel_lhs = lhs.clazz() == Expr::SEL;
	auto sel_rhs = rhs.clazz() == Expr::SEL;

	assert(rhs.clazz() != Expr::NIL || lhs.clazz() == Expr::SEL);
	auto index_lhs = mk_var_index(input, lhs, tid);
	auto index_rhs = mk_var_index(input, rhs, tid);

	// delegate calls
	if (!sel_lhs) if (!sel_rhs) return post_assignment_pointer_var_var(  cfg, index_lhs, index_rhs, tid, stmt);
	              else          return post_assignment_pointer_var_next( cfg, index_lhs, index_rhs, tid, stmt);
	else          if (!sel_rhs) return post_assignment_pointer_next_var( cfg, index_lhs, index_rhs, tid, stmt);
	              else          return post_assignment_pointer_next_next(cfg, index_lhs, index_rhs, tid, stmt);
}


/******************************** POINTER WTIH CFG ********************************/

#define NON_LOCAL(x) !(x >= cfg.shape->offset_locals(tid) && x < cfg.shape->offset_locals(tid) + cfg.shape->sizeLocals())
#define SHARED_VAR(x) (x >= cfg.shape->offset_program_vars() && x < cfg.shape->offset_locals(0))
; // this one is actually very useful: it fixes my syntax highlighting :)

static inline bool is_globally_reachable(const Shape& shape, std::size_t var, RelSet match=EQ_MT_GT) {
	for (auto i = shape.offset_program_vars(); i < shape.offset_locals(0); i++) {
		if (haveCommon(shape.at(i, var), match)) {
			return true;
		}
	}
	return false;
}

static inline const State* get_observer_state_for_shared(const Statement& stmt) {
	return stmt.function().prog().smr_observer().initial_state().states().at(0);
}

static inline void update_guard(DynamicSMRState& state, std::size_t dst, std::size_t src, bool copy_from_src, const Cfg& cfg, unsigned short tid) {
	// copies guard information or uses initial state for global addresses; provide a cfg with pc[tid] != null
	if (src >= cfg.shape->offset_program_vars() && src <= cfg.shape->offset_locals(0)) {
		// src is a shared pointer
		state.set(dst, get_observer_state_for_shared(*cfg.pc[tid]));
	} else {
		if (copy_from_src) state.set(dst, state.at(src));
		else state.set(dst, state.at(0)); // supposed to give default value
	}
}

static inline std::deque<std::pair<Shape*, bool>> split_on_global_reach(const Shape& shape, std::size_t var) {
	std::deque<std::pair<Shape*, bool>> result;
	auto remaining = std::make_unique<Shape>(shape);
	for (std::size_t i = shape.offset_program_vars(); i < shape.offset_locals(0); i++) {
		Shape* globreach = isolate_partial_concretisation(*remaining, i, var, EQ_MT_GT);
		if (globreach) result.emplace_back(globreach, true);
		remaining.reset(isolate_partial_concretisation(*remaining, i, var, MF_GF_BT));
		if (!remaining) break;
	}
	if (remaining) result.emplace_back(remaining.release(), false);
	return result;
}


std::vector<Cfg> tmr::post_assignment_pointer_var_var(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* stmt) {
	if (NON_LOCAL(lhs) && is_invalid_ptr(cfg, rhs)) raise_epr(cfg, rhs, "Bad assignment: spoiling non-local variable (ptr).");
	if (NON_LOCAL(lhs) && is_invalid_next(cfg, rhs)) raise_epr(cfg, rhs, "Bad assignment: spoiling non-local variable (next-field).");
	if (SHARED_VAR(lhs) && !cfg.own.at(rhs) && !is_globally_reachable(*cfg.shape, rhs)) raise_epr(cfg, rhs, "Invariant violation: pushing potentially retired address to shared heap.");

	Shape* shape = post_assignment_pointer_shape_var_var(*cfg.shape, lhs, rhs, stmt);
	auto result = mk_next_config_vec(cfg, shape, tid);
	Cfg& res = result.back();
	res.own.set(lhs, res.own.at(rhs));
	res.own.set(rhs, res.own.at(lhs)); // TODO: correct?
	res.valid_ptr.set(lhs, is_valid_ptr(cfg, rhs));
	res.valid_next.set(lhs, is_valid_next(cfg, rhs));
	update_guard(res.guard0state, lhs, rhs, true, cfg, tid);
	update_guard(res.guard1state, lhs, rhs, true, cfg, tid);
	return result;
}

std::vector<Cfg> tmr::post_assignment_pointer_var_next(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* stmt) {
	if (is_invalid_ptr(cfg, rhs)) raise_rpr(cfg, rhs, "Dereference of invalid pointer.");
	if (NON_LOCAL(lhs) && is_invalid_next(cfg, rhs)) raise_epr(cfg, rhs, "Bad assignment: spoiling non-local variable.");
	if (SHARED_VAR(lhs) || is_globally_reachable(*cfg.shape, lhs)) {
		bool shared = is_globally_reachable(*cfg.shape, rhs);
		bool owned = cfg.own.at(rhs);
		bool owned_succ = is_globally_reachable(*cfg.shape, rhs, EQ_MF_GF) || cfg.shape->test(rhs, cfg.shape->index_NULL(), EQ); // next reaches shared or null
		if (!shared && !(owned && owned_succ)) raise_epr(cfg, rhs, "Invariant violation: pushing potentially retired address to shared heap.");
	}

	auto tmp = split_on_global_reach(*cfg.shape, rhs);
	std::vector<Cfg> result;
	result.reserve(tmp.size());

	for (std::pair<Shape*, bool> pair : tmp) {
		Shape* tmpshape = pair.first;
		bool rhs_shared_reachable = pair.second;

		Shape* shape = post_assignment_pointer_shape_var_next(*tmpshape, lhs, rhs, stmt);
		delete tmpshape;

		result.push_back(mk_next_config(cfg, shape, tid));
		Cfg& res = result.back();

		res.own.set(lhs, false); // approximation
		if (rhs_shared_reachable) {
			res.valid_ptr.set(lhs, true);
			res.valid_next.set(lhs, true);
			res.guard0state.set(lhs, get_observer_state_for_shared(*cfg.pc[tid]));
			res.guard1state.set(lhs, get_observer_state_for_shared(*cfg.pc[tid]));

		} else {
			res.valid_ptr.set(lhs, cfg.valid_next.at(rhs));
			res.valid_next.set(lhs, false); // is_globally_reachable(*cfg.shape, rhs)
			res.guard0state.set(lhs, nullptr);
			res.guard1state.set(lhs, nullptr);
		}
	}

	return result;
}

std::vector<Cfg> tmr::post_assignment_pointer_next_var(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* stmt) {
	if (is_invalid_ptr(cfg, lhs)) raise_rpr(cfg, lhs, "Bad assignment: dereference of invalid pointer.");
	if (is_globally_reachable(*cfg.shape, lhs) && is_invalid_ptr(cfg, rhs)) raise_epr(cfg, rhs, "Bad assignment: spoinling next field.");
	if (SHARED_VAR(lhs) || is_globally_reachable(*cfg.shape, lhs)) {
		bool owned = cfg.own.at(rhs);
		bool owned_succ = cfg.shape->test(rhs, cfg.shape->index_NULL(), MT); // next is null
		if (!owned || !owned_succ) raise_epr(cfg, rhs, "Invariant violation: pushing potentially retired address to shared heap. ##");
	}

	Shape* shape = post_assignment_pointer_shape_next_var(*cfg.shape, lhs, rhs, stmt);
	auto result = mk_next_config_vec(cfg, shape, tid);
	Cfg& res = result.back();
	// if we make rhs reachable from lhs, so we may publish it depending on the ownership of lhs
	if (res.own.at(rhs) && !res.own.at(lhs))
		res.own.set(rhs, false);
	res.valid_next.set(lhs, cfg.valid_ptr.at(rhs));
	return result;
}

std::vector<Cfg> tmr::post_assignment_pointer_next_next(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* stmt) {
	throw std::logic_error("Unsupported operation tmr::post_assignment_pointer_next_next");
}


/******************************** LHS = RHS ********************************/

Shape* tmr::post_assignment_pointer_shape_var_var(const Shape& input, const std::size_t lhs, const std::size_t rhs, const Statement* stmt) {
	/*  0  */	Shape* shape = new Shape(input);
	         	if (lhs == rhs) return shape;
	/*  1  */	shape->set(lhs, rhs, singleton(EQ));
	/*  2  */	for (std::size_t i = 0; i < shape->size(); i++)
	         		shape->set(lhs, i, shape->at(rhs, i));
	         	return shape;
}


/******************************** LHS = RHS.next ********************************/

Shape* tmr::post_assignment_pointer_shape_var_next(const Shape& input, const std::size_t lhs, const std::size_t rhs, const Statement* stmt) {
	CHECK_RPRF_ws(rhs, stmt);
	CHECK_ACCESS_ws(rhs, stmt);

	Shape* result = new Shape(input);
	assert(consistent(*result));

	result->set(lhs, result->index_NULL(), EQ_MT_GT_BT);
	// result->set(lhs, result->index_FREE(), /*EQ_*/MT_GT_BT);
	result->set(lhs, result->index_UNDEF(), MT_GT_BT);

	for (std::size_t i = result->offset_vars(); i < result->size(); i++)
		result->set(lhs, i, PRED);
	result->set(rhs, lhs, MT_);
	result->set(lhs, result->index_REUSE(), PRED);

	bool needs_iterating;
	do {
		needs_iterating = false;
		for (std::size_t i = 0; i < result->size(); i++) {
			if (i == rhs) continue;
			for (Rel r : result->at(lhs, i))
				if (!consistent(*result, lhs, i, r)) {
					result->remove_relation(lhs, i, r);
					needs_iterating = true;
					assert(result->at(lhs, i).any());
				}
			}
	} while (needs_iterating);

	assert(result->at(rhs, lhs) == MT_);
	assert(consistent(*result));
	assert(is_closed_under_reflexivity_and_transitivity(*result));
	return result;
}


/******************************** LHS.next = RHS ********************************/

Shape* tmr::post_assignment_pointer_shape_next_var(const Shape& input, const std::size_t lhs, const std::size_t rhs, const Statement* stmt) {
	CHECK_RPRF_ws(lhs, stmt);
	CHECK_ACCESS_ws(lhs, stmt);

	// TODO: if lhs↦rhs is definite knowledge, then nothing to do, i.e. noop?

	/*  1  */	std::vector<Shape*> shapes = disambiguate(input, lhs);
	for (Shape* shape : shapes) {
		         	check_no_reachability(*shape, rhs, lhs, &input, stmt);
		         	// bool is_free = msetup == MM && shape->test(lhs, shape->index_FREE(), MT);
		/* 2+3 */	remove_successors(*shape, lhs); // TODO: why not before split?
		/*  4  */	// lhs↦rhs done by u=lhs and v=rhs in step 5
		/*  5  */	assert(shape->at(lhs, rhs) == BT_);
		         	for (std::size_t u = 0; u < shape->size(); u++) {
		         		if (!haveCommon(shape->at(u, lhs), EQ_MT_GT)) continue;
		         		for (std::size_t v = 0; v < shape->size(); v++) {
		         			if (!haveCommon(shape->at(rhs, v), EQ_MT_GT)) continue;
		         			
		         			RelSet result;
		         			RelSet ul = shape->at(u, lhs);
		         			RelSet rv = shape->at(rhs, v);

							assert(u != v);
							assert(ul.any());
							assert(rv.any());
							assert(!ul.test(BT) || ul.count() >= 2);
							assert(!rv.test(BT) || rv.count() >= 2);
							
							// u↦v if u=lhs and rhs=v
							if (ul.test(EQ) && rv.test(EQ)) result.set(MT);
							
							// u⇢v if u↦⇢lhs or rhs↦⇢v
							if (haveCommon(ul, MT_GT)) result.set(GT);
							else if (haveCommon(rv, MT_GT)) result.set(GT);
							
							// additionally u⋈v if u↤⇠⋈lhs or rhs↤⇠⋈v
							if (haveCommon(ul, MF_GF_BT)) result.set(BT);
							else if (haveCommon(rv, MF_GF_BT)) result.set(BT);

		         					shape->set(u, v, result);
		         				}
		         	}
		         	assert(shape->at(lhs, rhs) == MT_);
		         	// if (is_free) {
		         	//	const std::vector<std::size_t> free_index_vec = { shape->index_FREE() };
		         	//	auto eqVar = get_related(*shape, lhs, EQ_);
		         	//	auto preVar = get_related(*shape, lhs, MF_GF);
		         	//	extend_all(*shape, eqVar, free_index_vec, MT);
		         	//	extend_all(*shape, preVar, free_index_vec, GT);
		         	// }
		/* fin */	assert(is_closed_under_reflexivity_and_transitivity(*shape));
		         	assert(consistent(*shape));
	}
	/* fin */	return merge(shapes);
}


/******************************** LHS = NULL ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const NullAssignment& stmt, unsigned short tid) {
	CHECK_STMT;

	const Expr& le = stmt.lhs();
	const Shape& input = *cfg.shape;
	std::size_t lhs = mk_var_index(input, stmt.lhs(), tid);
	std::size_t rhs = input.index_NULL();

	Shape* shape;
	if (le.clazz() == Expr::VAR) {
		shape = post_assignment_pointer_shape_var_var(input, lhs, rhs);
	} else {
		assert(le.clazz() == Expr::SEL);
		if (is_invalid_ptr(cfg, lhs)) raise_rpr(cfg, lhs, "Bad assignment: dereference of invalid pointer.");
		shape = post_assignment_pointer_shape_next_var(input, lhs, rhs, &stmt);
	}

	std::vector<Cfg> result;
	result.push_back(mk_next_config(cfg, shape, tid));
	if (le.clazz() == Expr::VAR) {
		result.back().valid_ptr.set(lhs, true);
		result.back().valid_next.set(lhs, true); // TODO: correct?
	} else {
		result.back().valid_next.set(lhs, true);
	}
	// TODO: what about next fields?
	return result;
}
