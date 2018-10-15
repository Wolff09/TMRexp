#include "post/eval.hpp"

#include "../helpers.hpp"
#include "post/helpers.hpp"

using namespace tmr;

static const std::array<std::array<RelSet, 3>, 3> RELSET_LOOKUP =  {{ {{BT_, EQ_, MF_}}, {{EQ_, EQ_, MF_}}, {{MT_, MT_, BT_}} }}; // BT used as dummy

static inline bool is_shared_var(const Shape& shape, std::size_t var) {
	return shape.offset_program_vars() <= var && var < shape.offset_locals(0);
}

static inline void check_deref(const Cfg& cfg, const Shape& shape, const Expr& expr, std::size_t var, unsigned short tid) {
	if (expr.clazz() == Expr::SEL) {
		check_ptr_access(shape, var);
	}
}

std::pair<Shape*, Shape*> tmr::eval_eqneq(const Cfg& cfg, const Shape& input, const Expr& le, const Expr& re, bool inverted, unsigned short tid) {
	assert(le.type() == POINTER);
	assert(re.type() == POINTER);

	// get cell term indexes for le and re
	std::size_t lhs = mk_var_index(input, le, tid);
	std::size_t rhs = mk_var_index(input, re, tid);
	if (lhs == rhs) return { new Shape(input), NULL };

	check_deref(cfg, input, le, lhs, tid);
	check_deref(cfg, input, re, rhs, tid);

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
	}

	// false branch
	if (sp.second != NULL) {
		result.push_back(mk_next_config(cfg, sp.second, nN, tid));
	}

	return result;
}
