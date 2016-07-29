#include "post/assign.hpp"
#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"

using namespace tmr;


std::vector<Cfg> tmr::post(const Cfg& cfg, const Assignment& stmt, unsigned short tid, MemorySetup msetup) {
	CHECK_STMT;

	const Expr& lhs = stmt.lhs();
	const Expr& rhs = stmt.rhs();

	Cfg res = stmt.lhs().type() == DATA ? post_assignment_data(   cfg, lhs, rhs, tid, msetup, &stmt)
	                                    : post_assignment_pointer(cfg, lhs, rhs, tid, msetup, &stmt);

	if (stmt.fires_lp()) {
		// evaluate the condition, branch if needed, then fire
		// this is also present in linp.cpp => reuse

		// pc in res was already advanced, should be the linearization point
		assert(res.pc[tid] == &stmt.lp());

		// do a post for the linearization point
		return tmr::post(res, stmt.lp(), tid, msetup);
	} else {
		// nothing to fire => nothing to do
		std::vector<Cfg> result;
		result.push_back(std::move(res));
		return result;
	}
}


/******************************** DATA: LHS.DATA = RHS.DATA ********************************/

Cfg tmr::post_assignment_data(const Cfg& cfg, const Expr& lhs, const Expr& rhs, unsigned short tid, MemorySetup msetup, const Statement* stmt) {
	assert(false);
	throw std::logic_error("Malicious call to tmr::post_assignment_data()");
}


/******************************** POINTER ********************************/


Cfg tmr::post_assignment_pointer(const Cfg& cfg, const Expr& lhs, const Expr& rhs, unsigned short tid, MemorySetup msetup, const Statement* stmt) {
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
	if (!sel_lhs) if (!sel_rhs) return post_assignment_pointer_var_var(  cfg, index_lhs, index_rhs, tid, msetup, stmt);
	              else          return post_assignment_pointer_var_next( cfg, index_lhs, index_rhs, tid, msetup, stmt);
	else          if (!sel_rhs) return post_assignment_pointer_next_var( cfg, index_lhs, index_rhs, tid, msetup, stmt);
	              else          return post_assignment_pointer_next_next(cfg, index_lhs, index_rhs, tid, msetup, stmt);
}


/******************************** POINTER WTIH CFG ********************************/

#define NON_LOCAL(x) !(x >= cfg.shape->offset_locals(tid) && x < cfg.shape->offset_locals(tid) + cfg.shape->sizeLocals())
; // this one is actually very useful: it fixes my syntax highlighting :)

static inline bool is_globally_reachable(const Shape& shape, std::size_t var) {
	for (auto i = shape.offset_program_vars(); i < shape.offset_locals(0); i++) {
		if (haveCommon(shape.at(i, var), EQ_MT_GT)) {
			return true;
		}
	}
	return false;
}


Cfg tmr::post_assignment_pointer_var_var(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const MemorySetup msetup, const Statement* stmt) {
	if (msetup == PRF && NON_LOCAL(lhs) && (cfg.sin[rhs] || is_invalid(cfg, rhs))) raise_eprf(cfg, rhs, "Bad assignment: spoiling non-local variable");
	if (msetup == PRF && cfg.sin[rhs]) raise_eprf(cfg, rhs, "Bad assignment: accessing strongly invalid pointer");
	Shape* shape = post_assignment_pointer_shape_var_var(*cfg.shape, lhs, rhs, msetup, stmt);
	Cfg res = mk_next_config(cfg, shape, tid);
	set_age_equal(res, lhs, rhs);
	res.own.set_ownership(lhs, res.own.is_owned(rhs));
	// TODO: this could publish rhs
	res.sin[lhs] = res.sin[rhs];
	res.invalid[lhs] = is_invalid(cfg, rhs) || res.sin[rhs];
	return res;
}

Cfg tmr::post_assignment_pointer_var_next(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const MemorySetup msetup, const Statement* stmt) {
	if (msetup == PRF && NON_LOCAL(lhs) && (cfg.sin[rhs] || is_invalid(cfg, rhs))) raise_eprf(cfg, rhs, "Bad assignment: spoiling non-local variable");
	if (msetup == PRF && cfg.sin[rhs]) raise_eprf(cfg, rhs, "Bad assignment: accessing strongly invalid pointer");
	Shape* shape = post_assignment_pointer_shape_var_next(*cfg.shape, lhs, rhs, msetup, stmt);
	Cfg res = mk_next_config(cfg, shape, tid);
	// res.ages->set(lhs, rhs, AgeRel::BOT);
	// set_age_equal(res, lhs, 0);
	for (std::size_t i = 0; i < cfg.shape->size(); i++) {
		res.ages->set_real(lhs, i, cfg.ages->at_next(rhs, i));
		res.ages->set_next(lhs, i, AgeRel::BOT);
	}
	res.ages->set_real(lhs, lhs, AgeRel::EQ);
	res.ages->set_next(lhs, lhs, AgeRel::EQ);
	res.ages->set(lhs, false, rhs, true, AgeRel::EQ);
	res.own.publish(lhs); // overapproximation
	// check ownership of lhs
	// bool owned = true;
	// for (std::size_t i = 3; i < shape->size(); i++) {
	//	if (i == lhs) continue;
	//	if (i >= shape->offset_locals(tid) && i < shape->offset_locals(tid) + shape->sizeLocals()) continue;
	//	if (haveCommon(shape->at(i, lhs), EQ_MT_GT) && !res.own.is_owned(i)) {
	//		owned = false;
	//		break;
	//	}
	// }
	// res.own.set_ownership(lhs, owned);
	res.sin[lhs] = res.sin[rhs] || is_invalid(cfg, rhs);
	res.invalid[lhs] = res.sin[rhs] || is_invalid(cfg, rhs);
	return res;
}

Cfg tmr::post_assignment_pointer_next_var(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const MemorySetup msetup, const Statement* stmt) {
	if (msetup == PRF && NON_LOCAL(lhs) && (cfg.sin[rhs] || is_invalid(cfg, rhs))) raise_eprf(cfg, lhs, "Bad assignment: spoiling non-local variable.");
	if (msetup == PRF && is_globally_reachable(*cfg.shape, lhs) && (cfg.sin[rhs] || is_invalid(cfg, rhs))) raise_eprf(cfg, lhs, "Bad assignment: spoiling globally reachable next field.");
	// if (cfg.sin[lhs] || is_invalid(cfg, lhs)) raise_eprf(cfg, lhs, "Bad assignment: write to (strongly) invalid next field."); // TODO: removal okay?
	if (msetup == PRF && cfg.sin[lhs]) raise_eprf(cfg, rhs, "Bad assignment: accessing strongly invalid pointer");
	Shape* shape = post_assignment_pointer_shape_next_var(*cfg.shape, lhs, rhs, msetup, stmt);
	Cfg res = mk_next_config(cfg, shape, tid);
	// no age update since we are setting the next field and are not touching the pointer/cell itself
	// if we make rhs reachable from lhs, we may publish it depending on the ownership of lhs
	if (res.own.is_owned(rhs) && !res.own.is_owned(lhs))
		res.own.publish(rhs);
	return res;
}

Cfg tmr::post_assignment_pointer_next_next(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const MemorySetup msetup, const Statement* stmt) {
	assert(false);
	throw std::logic_error("Malicious call to tmr::post_assignment_pointer_next_next()");
}


/******************************** LHS = RHS ********************************/

Shape* tmr::post_assignment_pointer_shape_var_var(const Shape& input, const std::size_t lhs, const std::size_t rhs, const MemorySetup msetup, const Statement* stmt) {
	/*  0  */	Shape* shape = new Shape(input);
	         	if (lhs == rhs) return shape;
	/*  1  */	shape->set(lhs, rhs, singleton(EQ));
	/*  2  */	for (std::size_t i = 0; i < shape->size(); i++)
	         		shape->set(lhs, i, shape->at(rhs, i));
	/* fin */	assert(consistent(*shape));
	         	return shape;
}


/******************************** LHS = RHS.next ********************************/

Shape* tmr::post_assignment_pointer_shape_var_next(const Shape& input, const std::size_t lhs, const std::size_t rhs, const MemorySetup msetup, const Statement* stmt) {
	CHECK_PRF_ws(rhs, stmt);
	CHECK_ACCESS_ws(rhs, stmt);

	Shape* result = new Shape(input);
	assert(consistent(*result));

	result->set(lhs, result->index_NULL(), EQ_MT_GT_BT);
	result->set(lhs, result->index_FREE(), /*EQ_*/MT_GT_BT);
	result->set(lhs, result->index_UNDEF(), MT_GT_BT);

	for (std::size_t i = result->offset_vars(); i < result->size(); i++)
		result->set(lhs, i, PRED);
	result->set(rhs, lhs, MT_);

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

Shape* tmr::post_assignment_pointer_shape_next_var(const Shape& input, const std::size_t lhs, const std::size_t rhs, const MemorySetup msetup, const Statement* stmt) {
	CHECK_PRF_ws(lhs, stmt);
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


/******************************** LHS.next = RHS.next ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const NullAssignment& stmt, unsigned short tid, MemorySetup msetup) {
	CHECK_STMT;

	// TODO: what about ages?
	// TODO: what about sins?

	const Expr& le = stmt.lhs();
	const Shape& input = *cfg.shape;
	std::size_t lhs = mk_var_index(input, stmt.lhs(), tid);
	std::size_t rhs = input.index_NULL();

	Shape* shape;
	if (le.clazz() == Expr::VAR) {
		shape = post_assignment_pointer_shape_var_var(input, lhs, rhs, msetup);
	} else {
		assert(le.clazz() == Expr::SEL);
		if (msetup == PRF && (is_invalid(cfg, lhs) || cfg.sin[lhs])) raise_eprf(cfg, lhs, "Bad assignment: write to (strongly) invalid next field.");
		shape = post_assignment_pointer_shape_next_var(input, lhs, rhs, msetup, &stmt);
	}

	std::vector<Cfg> result;
	result.push_back(mk_next_config(cfg, shape, tid));
	if (le.clazz() == Expr::VAR) result.back().invalid[lhs] = false;
	if (le.clazz() == Expr::VAR) result.back().sin[lhs] = false;
	return result;
}
