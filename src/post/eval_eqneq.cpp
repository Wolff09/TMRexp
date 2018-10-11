#include "post/eval.hpp"

#include "../helpers.hpp"
#include "post/helpers.hpp"

using namespace tmr;

static const std::array<std::array<RelSet, 3>, 3> RELSET_LOOKUP =  {{ {{BT_, EQ_, MF_}}, {{EQ_, EQ_, MF_}}, {{MT_, MT_, BT_}} }}; // BT used as dummy

static inline bool is_shared_var(const Shape& shape, std::size_t var) {
	return shape.offset_program_vars() <= var && var < shape.offset_locals(0);
}

static inline bool is_retired_var(const Cfg& cfg, std::size_t var) {
	bool g0retire = cfg.guard0state.at(var) && cfg.guard0state.at(var)->is_special();
	bool g1retire = cfg.guard1state.at(var) && cfg.guard1state.at(var)->is_special();
	return g0retire || g1retire;
}

static inline void check_deref(const Cfg& cfg, const Shape& shape, const Expr& expr, std::size_t var, unsigned short tid) {
	if (expr.clazz() == Expr::SEL) {
		if (is_invalid_ptr(cfg, var)) {
			raise_rpr(cfg, var, "Dereferencing invalid pointer.");
		}
		check_ptr_access(shape, var);
	}
}

std::pair<Shape*, Shape*> tmr::eval_eqneq(const Cfg& cfg, const Shape& input, const Expr& le, const Expr& re, bool inverted, unsigned short tid, bool allow_invalid) {
	assert(le.type() == POINTER);
	assert(re.type() == POINTER);

	// get cell term indexes for le and re
	std::size_t lhs = mk_var_index(input, le, tid);
	std::size_t rhs = mk_var_index(input, re, tid);
	if (lhs == rhs) return { new Shape(input), NULL };

	check_deref(cfg, input, le, lhs, tid);
	check_deref(cfg, input, re, rhs, tid);

	if (!allow_invalid) {
		if (is_invalid(cfg, le, tid) || is_invalid(cfg, re, tid)) {
			std::cout << "Comparison with invalid pointer/expression" << std::endl;
			std::cout << cfg << *cfg.shape << std::endl;
			throw std::runtime_error("Comparison with invalid pointer.");
		}
	}

	RelSet sing = RELSET_LOOKUP[le.clazz()][re.clazz()];
	// split heap and decide which shape enters which branch
	Shape* eq = isolate_partial_concretisation(input, lhs, rhs, sing);
	Shape* neq = isolate_partial_concretisation(input, lhs, rhs, sing.flip());

	if (inverted) return { neq, eq };
	else return { eq, neq };
}

std::vector<Cfg> tmr::eval_cond_eqneq(const Cfg& cfg, const EqNeqCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid) {
	auto& le = cond.lhs();
	auto& re = cond.rhs();
	bool is_inverted = cond.is_inverted();
	auto sp = eval_eqneq(cfg, le, re, is_inverted, tid);
	
	std::vector<Cfg> result;
	result.reserve(2);
	
	// true branch
	if (sp.first != NULL) {
		result.push_back(mk_next_config(cfg, sp.first, nY, tid));

		// if it is a comparison with =, then validity may be updated
		if (!is_inverted) {
			Cfg& cf = result.back();
			std::size_t lhs = mk_var_index(*cf.shape, le, tid);
			std::size_t rhs = mk_var_index(*cf.shape, re, tid);
			if (cf.valid_ptr.at(lhs) ^ cf.valid_ptr.at(rhs)) {
				cf.valid_ptr.set(lhs, true);
				cf.valid_ptr.set(rhs, true);
			}

			// Prune false-positive post image (the false-positive can be harmful for the analysis)
			bool is_shared = is_shared_var(*cf.shape, lhs) || is_shared_var(*cf.shape, rhs);
			bool is_retired = is_retired_var(cf, lhs) || is_retired_var(cf, rhs);
			if (is_shared && is_retired) {
				/* This case is a false-positive due to the invariant that shared objects are never retired.
				 * We check the invariant whenever the shared heap is modified.
				 * Hence, the fact that here a shared object is shared must be due to the coarse shape analysis and shape merging.
				 * ==> We simply remove the constructed post image as it cannot occur.
				 */
				result.pop_back();
			}
		}
	}

	// false branch
	if (sp.second != NULL) {
		result.push_back(mk_next_config(cfg, sp.second, nN, tid));
	}

	return result;
}
