#include "post/eval.hpp"

#include "../helpers.hpp"
#include "post/helpers.hpp"

using namespace tmr;

static const std::array<std::array<RelSet, 3>, 3> RELSET_LOOKUP =  {{ {{BT_, EQ_, MF_}}, {{EQ_, EQ_, MF_}}, {{MT_, MT_, BT_}} }}; // BT used as dummy

std::pair<Shape*, Shape*> tmr::eval_eqneq(const Cfg& cfg, const Shape& input, const Expr& le, const Expr& re, bool inverted, unsigned short tid) {
	assert(le.type() == POINTER);
	assert(re.type() == POINTER);

	// get cell term indexes for le and re
	std::size_t lhs = le.clazz() == Expr::NIL ? input.index_NULL() : mk_var_index(input, le, tid);
	std::size_t rhs = re.clazz() == Expr::NIL ? input.index_NULL() : mk_var_index(input, re, tid);

	if (lhs == rhs) return { new Shape(input), NULL };

	// Rel rel;
	//      if (le.clazz() == Expr::NIL && re.clazz() == Expr::VAR) rel = EQ;
	// else if (le.clazz() == Expr::NIL && re.clazz() == Expr::SEL) rel = MF;
	// else if (le.clazz() == Expr::VAR && re.clazz() == Expr::NIL) rel = EQ;
	// else if (le.clazz() == Expr::VAR && re.clazz() == Expr::VAR) rel = EQ;
	// else if (le.clazz() == Expr::VAR && re.clazz() == Expr::SEL) rel = MF;
	// else if (le.clazz() == Expr::SEL && re.clazz() == Expr::NIL) rel = MT;
	// else if (le.clazz() == Expr::SEL && re.clazz() == Expr::VAR) rel = MT;
	// else throw std::logic_error("Malicious call to tmr::eval_eqneq()");
	// RelSet sing = singleton(rel);
	RelSet sing = RELSET_LOOKUP[le.clazz()][re.clazz()];

	// split heap and decide which shape enters which branch
	Shape* eq = isolate_partial_concretisation(input, lhs, rhs, sing);
	Shape* neq = isolate_partial_concretisation(input, lhs, rhs, sing.flip());

	if (inverted) return { neq, eq };
	else return { eq, neq };
}

std::vector<Cfg> tmr::eval_cond_eqneq(const Cfg& cfg, const EqNeqCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid) {
	auto sp = eval_eqneq(cfg, cond.lhs(), cond.rhs(), cond.is_inverted(), tid);
	
	std::vector<Cfg> result;
	result.reserve((sp.first != NULL ? 1 : 0) + (sp.second != NULL ? 1 : 0));
	
	if (sp.first != NULL)
		result.push_back(mk_next_config(cfg, sp.first, nY, tid));
	if (sp.second != NULL)
		result.push_back(mk_next_config(cfg, sp.second, nN, tid));

	return result;
}
