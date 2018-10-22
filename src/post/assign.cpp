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

	return result;
}


/******************************** DATA: LHS.DATA = RHS.DATA ********************************/

std::vector<Cfg> tmr::post_assignment_data(const Cfg& cfg, const Expr& lhs, const Expr& rhs, unsigned short tid, const Statement* stmt) {
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

inline void remove_ownership(Cfg& cfg, std::size_t index, unsigned short tid) {
	if (index == cfg.shape->offset_locals(tid) && cfg.shape->sizeLocals() > 0) {
		cfg.owned[tid] = false;
	}
}

std::vector<Cfg> tmr::post_assignment_pointer_var_var(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* stmt) {
	Shape* shape = post_assignment_pointer_shape_var_var(*cfg.shape, lhs, rhs, stmt);
	auto result = mk_next_config_vec(cfg, shape, tid);
	remove_ownership(result.back(), rhs, tid);
	return result;
}

std::vector<Cfg> tmr::post_assignment_pointer_var_next(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* stmt) {
	Shape* shape = post_assignment_pointer_shape_var_next(*cfg.shape, lhs, rhs, stmt);
	auto result = mk_next_config_vec(cfg, shape, tid);
	return result;
}

std::vector<Cfg> tmr::post_assignment_pointer_next_var(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* stmt) {
	Shape* shape = post_assignment_pointer_shape_next_var(*cfg.shape, lhs, rhs, stmt);
	auto result = mk_next_config_vec(cfg, shape, tid);
	remove_ownership(result.back(), rhs, tid);
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
	check_ptr_access(input, rhs, stmt);

	if (lhs == rhs) {
		throw std::logic_error("Cannot assign to variable from its own next selector.");
	}

	Shape* result = new Shape(input);

	result->set(lhs, result->index_NULL(), EQ_MT_GT_BT);
	result->set(lhs, result->index_UNDEF(), MT_GT_BT);

	for (std::size_t i = result->offset_vars(); i < result->size(); i++)
		result->set(lhs, i, PRED);
	result->set(rhs, lhs, MT_);
	result->set(lhs, result->index_REC(), PRED);

	bool needs_iterating;
	do {
		needs_iterating = false;
		for (std::size_t i = 0; i < result->size(); i++) {
			if (i == rhs) continue;
			for (Rel r : result->at(lhs, i))
				if (!consistent(*result, lhs, i, r)) {
					result->remove_relation(lhs, i, r);
					needs_iterating = true;
				}
			}
	} while (needs_iterating);

	return result;
}


/******************************** LHS.next = RHS ********************************/

Shape* tmr::post_assignment_pointer_shape_next_var(const Shape& input, const std::size_t lhs, const std::size_t rhs, const Statement* stmt) {
	check_ptr_access(input, lhs, stmt);

	// TODO: if lhs↦rhs is definite knowledge, then nothing to do, i.e. noop?

	/*  1  */	std::vector<Shape*> shapes = disambiguate(input, lhs);
	for (Shape* shape : shapes) {
		         	check_no_reachability(*shape, rhs, lhs, &input, stmt);
		/* 2+3 */	remove_successors(*shape, lhs); // TODO: why not before split?
		/*  4  */	// lhs↦rhs done by u=lhs and v=rhs in step 5
		/*  5  */	for (std::size_t u = 0; u < shape->size(); u++) {
		         		if (!haveCommon(shape->at(u, lhs), EQ_MT_GT)) continue;
		         		for (std::size_t v = 0; v < shape->size(); v++) {
		         			if (!haveCommon(shape->at(rhs, v), EQ_MT_GT)) continue;
		         			
		         			RelSet result;
		         			RelSet ul = shape->at(u, lhs);
		         			RelSet rv = shape->at(rhs, v);

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

	if (le.type() != POINTER) {
		throw std::logic_error("NullAssignment to data not supported");
	}

	Shape* shape;

	if (le.clazz() == Expr::VAR) {
		// stmt: var = NULL
		shape = post_assignment_pointer_shape_var_var(input, lhs, rhs);

	} else {
		assert(le.clazz() == Expr::SEL);
		// stmt: ptr->next = NULL
		shape = post_assignment_pointer_shape_next_var(input, lhs, rhs, &stmt);	
	}

	return mk_next_config_vec(cfg, shape, tid);
}


/******************************** SET OPERATIONS ********************************/

inline MultiSet& getsetref(Cfg& cfg, std::size_t setid) {
	switch (setid) {
		case 0: return cfg.dataset0;
		case 1: return cfg.dataset1;
		case 2: return cfg.dataset2;
		default: throw std::logic_error("Unsupported dataset reference.");
	}
}

inline DataValue getdatasel(const Cfg& cfg, std::size_t selid) {
	switch (selid) {
		case 0: return cfg.datasel0;
		case 1: return cfg.datasel1;
		default: throw std::logic_error("Unsupported data selector.");
	}
}

inline void addtoset(Cfg& cfg, std::size_t setid, DataValue val, unsigned short tid) {
	auto& dst = getsetref(cfg, setid);

	switch (val) {
		case DataValue::DATA: dst[tid] = DataSet::WITH_DATA; break;
		case DataValue::OTHER: /* leave unchanged */ break;
	}
}

inline void setcombine(Cfg& cfg, std::size_t lhsid, std::size_t rhsid, SetCombine::Type type, unsigned short tid) {
	auto& lhs = getsetref(cfg, lhsid);
	auto& rhs = getsetref(cfg, rhsid);

	switch (type) {
		case SetCombine::SETTO:
			lhs[tid] = rhs[tid];
			break;
		
		case SetCombine::UNION:
			throw std::logic_error("Set union not implemented.");

		case SetCombine::SUBTRACTION:
			switch (rhs[tid]) {
				case DataSet::WITH_DATA: lhs[tid] = DataSet::WITHOUT_DATA; break;
				case DataSet::WITHOUT_DATA: /* leave unchanged */ break;
			}
			break;
	}
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const SetCombine& stmt, unsigned short tid) {
	auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	setcombine(result.back(), stmt.lhs(), stmt.rhs(), stmt.type(), tid);
	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const SetClear& stmt, unsigned short tid) {
	auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	auto& set = getsetref(result.back(), stmt.setid());
	set[tid] = DataSet::WITHOUT_DATA;
	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const SetAddArg& stmt, unsigned short tid) {
	auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	addtoset(result.back(), stmt.setid(), cfg.arg[tid], tid);
	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const SetAddSel& stmt, unsigned short tid) {
	// TODO: sel id
	std::size_t lhs = mk_var_index(*cfg.shape, stmt.selector(), tid);

	Shape* eqsplit = isolate_partial_concretisation(*cfg.shape, lhs, cfg.shape->index_REC(), EQ_);
	Shape* neqsplit = isolate_partial_concretisation(*cfg.shape, lhs, cfg.shape->index_REC(), MT_GT_MF_GF_BT);

	std::vector<Cfg> result;
	result.reserve(3);

	if (eqsplit) {
		result.push_back(mk_next_config(cfg, eqsplit, tid));
		addtoset(result.back(), stmt.setid(), getdatasel(cfg, stmt.selector().index()), tid);
	}

	if (neqsplit) {
		result.push_back(mk_next_config(cfg, new Shape(*neqsplit), tid));
		addtoset(result.back(), stmt.setid(), DataValue::DATA, tid);
		result.push_back(mk_next_config(cfg, neqsplit, tid));
		addtoset(result.back(), stmt.setid(), DataValue::OTHER, tid);
	}

	return result;
}

/******************************** REC INIT ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const InitRecPtr& stmt, unsigned short tid) {
	auto rhs = mk_var_index(*cfg.shape, stmt.rhs(), tid);
	
	if (rhs != cfg.shape->offset_locals(tid)) {
		// ensure that we are tracking ownership
		throw std::logic_error("__rec__ must be initialized from the first thread-local varialbe.");
	}
	// if (cfg.owned[tid]) {
	// 	throw std::logic_error("__rec__ must be initialized from a definitely owned pointer.");
	// }
	
	Shape* shape;
	if (cfg.offender[tid]) {
		// shape->REC() holds offending threads record
		shape = post_assignment_pointer_shape_var_var(*cfg.shape, cfg.shape->index_REC(), rhs, &stmt);

	} else {
		// we ignore non-offending threads records
		shape = new Shape(*cfg.shape);
	}

	auto result = mk_next_config_vec(cfg, shape, tid);
	result.back().owned[tid] = false;
	return result;
}
