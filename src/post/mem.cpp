#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include <deque>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/setout.hpp"
#include "post/assign.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** MALLOC ********************************/

#define NON_LOCAL(x) !(x >= cfg.shape->offset_locals(tid) && x < cfg.shape->offset_locals(tid) + cfg.shape->sizeLocals())
; // this semicolon is actually very useful: it fixes my syntax highlighting :)

std::vector<Cfg> tmr::post(const Cfg& cfg, const Malloc& stmt, unsigned short tid) {
	CHECK_STMT;
	
	const Shape& input = *cfg.shape;
	const auto var_index = mk_var_index(input, stmt.decl(), tid);
	auto smr_init = stmt.function().prog().smr_observer().initial_state().states().at(0);

	if (NON_LOCAL(var_index) && &stmt.function().prog().init_fun() != &stmt.function()) {
			throw std::runtime_error("Allocations may not target global variables.");
	}

	std::vector<Cfg> result;

	/* malloc gives a fresh memory cell */
	{
		auto shape1 = new Shape(input);
		for (std::size_t i = 0; i < input.size(); i++)
			shape1->set(var_index, i, BT_);
		shape1->set(var_index, var_index, EQ_);
		auto shape2 = post_assignment_pointer_shape_next_var(*shape1, var_index, input.index_NULL(), &stmt); // TODO: correct?
		delete shape1;

		result.push_back(mk_next_config(cfg, shape2, tid));
		Cfg& fresh = result.back();

		fresh.own.set(var_index, true);
		fresh.valid_ptr.set(var_index, true);
		fresh.valid_next.set(var_index, true);
		fresh.guard0state.set(var_index, smr_init);
		fresh.guard1state.set(var_index, smr_init);
	}


	/* malloc gives a freed cell */
	if (cfg.freed) {
		auto old = post_assignment_pointer_shape_var_var(input, var_index, input.index_REUSE(), &stmt);
		auto shape = post_assignment_pointer_shape_next_var(*old, var_index, input.index_NULL(), &stmt); // TODO: correct?
		delete old;

		// ensure that the newly allocated cell is not shared reachable -- invariant integrated into analysis
		for (std::size_t i = cfg.shape->offset_program_vars(); i < cfg.shape->offset_locals(0); i++) {
			if (!shape) break;
			old = shape;
			shape = isolate_partial_concretisation(*old, i, var_index, MF_GF_BT);
			delete old;
		}

		if (shape) {
			result.push_back(mk_next_config(cfg, shape, tid));
			Cfg& reuse = result.back();

			reuse.own.set(var_index, true);
			reuse.valid_ptr.set(var_index, true);
			reuse.valid_next.set(var_index, true);
			reuse.guard0state.set(var_index, smr_init);
			reuse.guard1state.set(var_index, smr_init);

			reuse.freed = false;
			reuse.retired = false;
		}
	}

	return result;
}


/******************************** FREE ********************************/

static Shape* extract_shared_unreachable(const Shape& shape, std::size_t var) {
	// extracts a subshape of the given one where var is not globally reachable, not null, and not udef; or null if no such shape exists
	Shape* old = tmr::isolate_partial_concretisation(shape, var, shape.index_NULL(), MT_GT_BT);
	if (!old) return NULL;
	Shape* result = tmr::isolate_partial_concretisation(*old, var, shape.index_UNDEF(), MT_GT_BT);
	delete old;
	for (std::size_t i = shape.offset_program_vars(); i < shape.offset_locals(0); i++) {
		if (!result) break; // happens if var is definitely reachable from some shared variable, definitely null, or definitely udef
		old = result;
		result = tmr::isolate_partial_concretisation(*old, i, var, MF_GF_BT); // shared i does not reach var
		delete old;
	}
	return result;
}

static inline std::deque<Shape*> split_shape_for_eq(const Shape& input, std::size_t begin, std::size_t end) {
	// same as in hp.cpp
	std::deque<Shape*> result;
	result.push_back(new Shape(input));
	for (std::size_t i = begin; i < end; i++) {
		for (std::size_t j = i+1; j < end; j++) {
			std::size_t size = result.size();
			for (std::size_t k = 0; k < size; k++) {
				Shape* shape = result.at(k);
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

static inline bool is_free_forbidden(const DynamicSMRState& state, std::size_t var, const Program& prog) {
	return state.at(var) && state.at(var)->next(prog.freefun(), OValue::Anonymous()).is_final();
}

static inline void fire_free_event(DynamicSMRState& state, std::size_t var, const Program& prog) {
	if (state.at(var)) {
		state.set(var, &state.at(var)->next(prog.freefun(), OValue::Anonymous()));
	}
}

static inline bool is_final(const DynamicSMRState& state, std::size_t var) {
	return state.at(var) && state.at(var)->is_final();
}

std::vector<Cfg> tmr::post_free(const Cfg& cfg, unsigned short tid, const Program& prog) {
	const Shape& input = *cfg.shape;
	std::vector<Cfg> result;

	for (std::size_t i = input.offset_locals(tid); i < input.offset_locals(tid) + input.sizeLocals(); i++) {
		if (cfg.own.at(i)) continue;
		if (is_free_forbidden(cfg.guard0state, i, prog)) continue;
		if (is_free_forbidden(cfg.guard1state, i, prog)) continue;
		/* Note:
		 * This check above is a quick check for whether or not the free is allowed.
		 * There might be an alias of the considered pointer which prohibits the free.
		 * We optimistically assume that this is not the case, and check later whether the free was wrong.
		 */
		
		auto tmp = extract_shared_unreachable(*cfg.shape, i);
		if (!tmp) continue;

		auto eqsplit = split_shape_for_eq(*tmp, 0, tmp->size());
		for (Shape* shape : eqsplit){
			result.push_back(Cfg(cfg, shape));
			auto& cf = result.back();

			// TODO: one could think about more precisely querying what becomes invalid
			for (std::size_t j = 0; j < cf.shape->size(); j++) {
				if (cf.shape->test(i, j, EQ)) {
					cf.valid_ptr.set(j, false);
					cf.valid_next.set(j, false);
					fire_free_event(cf.guard0state, j, prog);
					fire_free_event(cf.guard1state, j, prog);
					if (j == shape->index_REUSE()) {
						if (!cf.retired) {
							// TODO: correct?
							result.pop_back();
							break;
						}
						cf.freed = true;
						cf.retired = false;
					}
					if (is_final(cf.guard0state, j) || is_final(cf.guard1state, j)) {
						// Thorough check whether or not the free was allowed.
						// If we end up here, it was not.
						// In this case, we drop (ignore) the computed post image.
						result.pop_back();
						break;
					}
				}
			}
		}
	}

	return result;
}
