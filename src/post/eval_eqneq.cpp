#include "post/eval.hpp"

#include "../helpers.hpp"
#include "post/helpers.hpp"

using namespace tmr;


void check_ownership_violation(const Cfg& cfg, std::size_t lhs, std::size_t rhs) {
	if (cfg.own.is_owned(lhs) != cfg.own.is_owned(rhs) && lhs != cfg.shape->index_NULL() && rhs != cfg.shape->index_NULL()) {
		std::cout << "******************************************************" << std::endl;
		std::cout << "Malicious comparison: comparing owned with public cell" << std::endl;
		std::cout << "in cfg: " << cfg << *cfg.shape << *cfg.ages;
		throw std::runtime_error("Malicious comparison: comparing owned with public cell");
	}
}

std::pair<Shape*, Shape*> tmr::eval_eqneq(const Cfg& cfg, const Shape& input, const Expr& le, const Expr& re, bool inverted, unsigned short tid, bool check_own, bool check_eprf) {
	assert(le.type() == POINTER);
	assert(re.type() == POINTER);

	// get cell term indexes for le and re
	std::size_t lhs = le.clazz() == Expr::NIL ? input.index_NULL() : mk_var_index(input, le, tid);
	std::size_t rhs = re.clazz() == Expr::NIL ? input.index_NULL() : mk_var_index(input, re, tid);

	if (check_own)
		check_ownership_violation(cfg, lhs, rhs);
	if (check_eprf) {
	 	if (cfg.sin[lhs]) raise_eprf(cfg, lhs, "Bad comparison (lhs).");
	 	if (cfg.sin[rhs]) raise_eprf(cfg, rhs, "Bad comparison (rhs).");
	}	

	if (lhs == rhs) return { new Shape(input), NULL };

	Rel rel;
	     if (le.clazz() == Expr::NIL && re.clazz() == Expr::VAR) rel = EQ;
	else if (le.clazz() == Expr::NIL && re.clazz() == Expr::SEL) rel = MF;
	else if (le.clazz() == Expr::VAR && re.clazz() == Expr::NIL) rel = EQ;
	else if (le.clazz() == Expr::VAR && re.clazz() == Expr::VAR) rel = EQ;
	else if (le.clazz() == Expr::VAR && re.clazz() == Expr::SEL) rel = MF;
	else if (le.clazz() == Expr::SEL && re.clazz() == Expr::NIL) rel = MT;
	else if (le.clazz() == Expr::SEL && re.clazz() == Expr::VAR) rel = MT;
	else throw std::logic_error("Malicious call to tmr::eval_eqneq()");
	RelSet sing = singleton(rel);

	// split heap and decide which shape enters which branch
	Shape* eq = isolate_partial_concretisation(input, lhs, rhs, sing);
	Shape* neq = isolate_partial_concretisation(input, lhs, rhs, sing.flip());

	if (inverted) return { neq, eq };
	else return { eq, neq };
}

std::vector<Cfg> tmr::eval_cond_eqneq(const Cfg& cfg, const EqNeqCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid, MemorySetup msetup) {
	auto sp = eval_eqneq(cfg, cond.lhs(), cond.rhs(), cond.is_inverted(), tid, msetup);
	
	std::vector<Cfg> result;
	result.reserve((sp.first != NULL ? 1 : 0) + (sp.second != NULL ? 1 : 0));
	
	if (sp.first != NULL)
		result.push_back(mk_next_config(cfg, sp.first, nY, tid));
	if (sp.second != NULL)
		result.push_back(mk_next_config(cfg, sp.second, nN, tid));

	return result;
}


std::vector<Cfg> tmr::eval_cond_wage(const Cfg& cfg, const EqPtrAgeCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid, MemorySetup msetup) {
	std::size_t lhs = mk_var_index(*cfg.shape, cond.cond().lhs(), tid);
	std::size_t rhs = mk_var_index(*cfg.shape, cond.cond().rhs(), tid);

	if (cfg.ages->at(lhs, rhs) != AgeRel::BOT && cfg.ages->at(lhs, rhs) != AgeRel::EQ) {
		std::vector<Cfg> result;
		result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), nN, tid));
		return result;
	}

	auto sp = eval_eqneq(cfg, cond.cond().lhs(), cond.cond().rhs(), cond.cond().is_inverted(), tid, msetup);

	std::vector<Cfg> result;
	result.reserve((sp.first != NULL ? 1 : 0) + (sp.second != NULL ? 1 : 0));

	if (sp.first != NULL) {
		// check ages again (the above is only a shortcut to prevent heavy operations)
		if (cfg.ages->at(lhs, rhs) == AgeRel::BOT) {
			std::cout << "An age field misuse was detected (relation is undefined) in the following Condition: " << std::endl << "    " << cond;
			std::cout << std::endl << "For tid="<<tid<<" in the following configuration: " << std::endl << "    " << cfg << *cfg.shape << *cfg.ages << std::endl;
			delete sp.first;
			throw std::runtime_error("Age Field Misuse detected!");
			
		} else {
			const Statement* next = cfg.ages->at(lhs, rhs) == AgeRel::EQ ? nY : nN;
			result.push_back(mk_next_config(cfg, sp.first, next, tid));
		}
	}

	return result;
}
