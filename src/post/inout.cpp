#include "post.hpp"

#include <assert.h>
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/assign.hpp"

using namespace tmr;



/******************************** INPUT: LHS.DATA = __in__ ********************************/


void remove_observer_binding(Shape& shape, std::size_t obsct) {
	//convert observer variable id to cell term id
	obsct = shape.index_ObserverVar(obsct);
	// std::cout << "  - removing bindings for observer cell term " << obsct << std::endl;
	for (std::size_t i = 0; i < shape.size(); i++)
		shape.set(i, obsct, BT_);
	shape.set(obsct, obsct, EQ_);
	shape.set(obsct, shape.index_UNDEF(), MT_); // don't do this to distinguish between never seen and poped via the shape during interference
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const ReadInputAssignment& stmt, unsigned short tid) {
	CHECK_STMT;

	OValue in = cfg.inout[tid];
	assert(in.type() != OValue::EMPTY);
	assert(in.type() != OValue::DUMMY);

	const Shape& input = *cfg.shape;
	std::size_t lhs = mk_var_index(input, stmt.expr(), tid);

	std::vector<Cfg> result;

	/* Assigning a data value to lhs.data kills the value currently stored in lhs.data.
	 * Hence, we have to remove any binding of an observed value. More precisely,
	 * if lhs.data == z, where z is some observer value, and zct = lhs, where zct
	 * is the cell term corresponding to z, then we have to delete z from the heap.
	 * We do so by setting every relation of zct and t to zct~t for every cell term t,
	 * except for zct=zct.
	 * This should do the trick as we assume that lhs can be equal to at most one cell
	 * term representing an observer value, since we assume that observer variables
	 * represent disjoint equivalence classes.
	 * 
	 * Note that no matter what kind of value we store in lhs.data (observable, anonymous),
	 * it kills the current value of lhs.data.
	 * 
	 * Note that we need to disambiguate on lhs since cfg.shape might not contain lhs=zct as
	 * definite knowledge.
	 */
	std::vector<Shape*> shapes = disambiguate(input, lhs);
	// for (Shape* shape : shapes) std::cout << "Disambiguated: " << std::endl << *shape; std::cout << std::endl;
	for (Shape* shape : shapes)
		for (std::size_t oct = 0; oct < shape->sizeObservers(); oct++)
			if (shape->test(lhs, shape->index_ObserverVar(oct), EQ))
				remove_observer_binding(*shape, oct);

	assert(shapes.size() > 0);
	// both following cases must manage the memory consumed by shapes (either claim them or delete them)

	if (in.type() == OValue::OBSERVABLE) {
		// - cell term for observer variable with id=in.id() can observe this assignemnt
		//   no other cell term can as we assume that observer variables have different valuations
		//   hence, cell term obsvar[in.id()] now tracks the cell which is currently pointed to by lhs
		// - we assume that an input value is consumed only once
		//   but we cannot consume __in__ by setting cfg.inout[tid] = DUMMY to run into assertion errors for further
		//   readings since we need __in__ to emit an linearisation event
		std::size_t obs = input.index_ObserverVar(in.id());

		// execute assignment lhs=obs (beware, this already increases the PC)
		for (Shape* shape : shapes) {
			// Every observable value can only be seen once. Hence, one could think about ensuring that obs must not be
			// defined by now (shape[obs][UNDEF]=↦).
			// But: an interfering thread might add it! This would extend a configuration, and set shape[obs][UNDEF]=⋈.
			// The equality relation between the extended local variable obs gets then lost due to the projection from
			// the n-thread view to the n-1-thread view (n=2 or n=3). So we should also allow obs to be unrelated
			// (shape[obs][t]=⋈ for all t!=obs).
			// But we still run into trouble as the interference is playing at us... so just do the assignment and
			// do not assert anything.
			// /* removed on purpose (see comment above) */

			// TODO: the following should be "more correct" than the current setup, but it gives way more cfgs to explore
			// Cfg tmp(cfg, shape);
			// result.push_back(tmr::post_assignment_pointer_var_var(cfg, obs, lhs, tid, &stmt));

			Shape* res = tmr::post_assignment_pointer_shape_var_var(*shape, obs, lhs, &stmt);
			result.push_back(mk_next_config(cfg, res, tid));
			result.back().own.set_ownership(obs, result.back().own.is_owned(lhs));
			delete shape;
		}

	} else /* in.type() == OValue::ANONYMOUS */ {
		// no observer variable can observe this value
		// so we are not interested and do nothing at all
		// (we do not need to capture the cell pointed to by lhs with an observer cell term)
		for (Shape* shape : shapes)
			result.push_back(mk_next_config(cfg, shape, tid));
	}

	return result;
}


/******************************** OUTPUT: __out__ = RHS.DATA ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const WriteOutputAssignment& stmt, unsigned short tid) {
	CHECK_STMT;

	// do nothing... we have no explicit notion of data variables and returning
	std::vector<Cfg> result;
	result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), tid));
	return result;
}
