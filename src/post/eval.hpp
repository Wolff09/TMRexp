#pragma once

#include <stdexcept>
#include <assert.h>
#include "prog.hpp"
#include "shape.hpp"
#include "cfg.hpp"
#include "../post.hpp"
#include "post/helpers.hpp"


namespace tmr {

	std::pair<Shape*, Shape*> eval_eqneq(const Cfg& cfg, const Shape& shape, const Expr& le, const Expr& re, bool inverted, unsigned short tid);

	static std::pair<Shape*, Shape*> eval_eqneq(const Cfg& cfg, const Expr& le, const Expr& re, bool inverted, unsigned short tid) {
		return eval_eqneq(cfg, *cfg.shape, le, re, inverted, tid);
	}
	
	std::vector<Cfg> eval_cond_eqneq(const Cfg& cfg, const EqNeqCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid);
	
	std::vector<Cfg> eval_cond_cas(const Cfg& cfg, const CompareAndSwap& stmt, const Statement* nY, const Statement* nN, unsigned short tid);

	static std::vector<Cfg> eval_cond_cas(const Cfg& cfg, const CASCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid) {
		return eval_cond_cas(cfg, cond.cas(), nY, nN, tid);
	}

	static std::vector<Cfg> eval_cond_nondet(const Cfg& cfg, const NonDetCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid) {
		std::vector<Cfg> result;
		result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), nY, tid));
		result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), nN, tid));
		return result;
	}
	
	std::vector<Cfg> eval_epoch_var(const Cfg& cfg, const EpochVarCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid);
	
	std::vector<Cfg> eval_epoch_sel(const Cfg& cfg, const EpochSelCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid);

	static std::vector<Cfg> eval_cond(const Cfg& cfg, const Conditional& stmt, unsigned short tid) {
		const Statement* nextY = stmt.next_true_branch();
		const Statement* nextN = stmt.next_false_branch();
		switch (stmt.cond().type()) {
			case Condition::EQNEQ: return eval_cond_eqneq(cfg, static_cast<const EqNeqCondition&>(stmt.cond()), nextY, nextN, tid);
			case Condition::CASC: return eval_cond_cas(cfg, static_cast<const CASCondition&>(stmt.cond()), nextY, nextN, tid);
			case Condition::COMPOUND: throw std::logic_error("Compound conditions are not supported here (only in linearization points).");
			case Condition::ORACLEC: throw std::logic_error("Oracle conditions are not supported here (only in linearization points).");
			case Condition::NONDET: return eval_cond_nondet(cfg, static_cast<const NonDetCondition&>(stmt.cond()), nextY, nextN, tid);
			case Condition::EPOCH_VAR: return eval_epoch_var(cfg, static_cast<const EpochVarCondition&>(stmt.cond()), nextY, nextN, tid);
			case Condition::EPOCH_SEL: return eval_epoch_sel(cfg, static_cast<const EpochSelCondition&>(stmt.cond()), nextY, nextN, tid);
			case Condition::TRUEC:
				std::vector<Cfg> result;
				result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), nextY, tid));
				return result;
		}
	}

}
