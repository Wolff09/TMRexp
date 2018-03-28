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

void check_ownership_violation(const Cfg& cfg, std::size_t lhs, std::size_t rhs) {
	// if (cfg.own.at(lhs) != cfg.own.at(rhs) && lhs != cfg.shape->index_NULL() && rhs != cfg.shape->index_NULL()) {
	// 	std::cout << "******************************************************" << std::endl;
	// 	std::cout << "Malicious comparison: comparing owned with public cell" << std::endl;
	// 	std::cout << "in cfg: " << cfg << *cfg.shape << *cfg.ages;
	// 	throw std::runtime_error("Malicious comparison: comparing owned with public cell");
	// }
	// TODO: delete this
}

std::pair<Shape*, Shape*> tmr::eval_eqneq(const Cfg& cfg, const Shape& input, const Expr& le, const Expr& re, bool inverted, unsigned short tid, bool check_own, bool check_eprf) {
	assert(le.type() == POINTER);
	assert(re.type() == POINTER);

	// get cell term indexes for le and re
	// std::size_t lhs = le.clazz() == Expr::NIL ? input.index_NULL() : mk_var_index(input, le, tid);
	// std::size_t rhs = re.clazz() == Expr::NIL ? input.index_NULL() : mk_var_index(input, re, tid);
	std::size_t lhs = mk_var_index(input, le, tid);
	std::size_t rhs = mk_var_index(input, re, tid);

	if (check_own)
		check_ownership_violation(cfg, lhs, rhs);
	if (check_eprf) {
		// TODO: check for RPR here
	 	// if (cfg.sin[lhs]) raise_eprf(cfg, lhs, "Bad comparison (lhs).");
	 	// if (cfg.sin[rhs]) raise_eprf(cfg, rhs, "Bad comparison (rhs).");
	}	

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


// std::vector<Cfg> tmr::eval_cond_wage(const Cfg& cfg, const EqPtrAgeCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid) {
// 	std::size_t lhs = mk_var_index(*cfg.shape, cond.cond().lhs(), tid);
// 	std::size_t rhs = mk_var_index(*cfg.shape, cond.cond().rhs(), tid);
// 	bool lhs_next = cond.cond().lhs().clazz() == Expr::SEL;
// 	bool rhs_next = cond.cond().rhs().clazz() == Expr::SEL;

// 	if (cfg.ages->at(lhs, lhs_next, rhs, rhs_next) != AgeRel::BOT && cfg.ages->at(lhs, lhs_next, rhs, rhs_next) != AgeRel::EQ) {
// 		std::vector<Cfg> result;
// 		result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), nN, tid));
// 		return result;
// 	}

// 	auto sp = eval_eqneq(cfg, cond.cond().lhs(), cond.cond().rhs(), cond.cond().is_inverted(), tid);

// 	std::vector<Cfg> result;
// 	result.reserve((sp.first != NULL ? 1 : 0) + (sp.second != NULL ? 1 : 0));

// 	if (sp.first != NULL) {
// 		// check ages again (the above is only a shortcut to prevent heavy operations)
// 		if (cfg.ages->at(lhs, lhs_next, rhs, rhs_next) == AgeRel::BOT) {
// 			std::cout << "An age field misuse was detected (relation is undefined) in the following Condition: " << std::endl << "    " << cond;
// 			std::cout << std::endl << "For tid="<<tid<<" in the following configuration: " << std::endl << "    " << cfg << *cfg.shape << *cfg.ages << std::endl;
// 			delete sp.first;
// 			throw std::runtime_error("Age Field Misuse detected!");
			
// 		} else {
// 			const Statement* next = cfg.ages->at(lhs, lhs_next, rhs, rhs_next) == AgeRel::EQ ? nY : nN;
// 			result.push_back(mk_next_config(cfg, sp.first, next, tid));
// 		}
// 	}

// 	return result;
// }
