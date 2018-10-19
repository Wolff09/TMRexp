#include "post/assign.hpp"
#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include <deque>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/eval.hpp"

using namespace tmr;


std::vector<Cfg> tmr::post(const Cfg& cfg, const SetRecEpoch& stmt, unsigned short tid) {
	CHECK_STMT;
	auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	if (cfg.offender[tid]) {
		result.back().epochsel = result.back().localepoch[tid];
	}
	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const GetLocalEpochFromGlobalEpoch& stmt, unsigned short tid) {
	CHECK_STMT;
	auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	result.back().localepoch[tid] = result.back().globalepoch;
	return result;
}

std::vector<Cfg> tmr::eval_epoch_var(const Cfg& cfg, const EpochVarCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid) {
	// checks: epoch != Epoch
	std::vector<Cfg> result;
	result.push_back(cfg.copy());
	bool cres = cfg.localepoch[tid] != cfg.globalepoch;
	result.back().pc[tid] = cres ? nY : nN;
	return result;
}

std::vector<Cfg> tmr::eval_epoch_sel(const Cfg& cfg, const EpochSelCondition& cond, const Statement* nY, const Statement* nN, unsigned short tid) {
	// checks: epoch != __rec__->epoch
	std::size_t var = mk_var_index(*cfg.shape, cond.var(), tid);
	std::vector<Cfg> result;
	result.reserve(3);

	Shape* eqsplit = isolate_partial_concretisation(*cfg.shape, var, cfg.shape->index_REC(), EQ_);
	Shape* neqsplit = isolate_partial_concretisation(*cfg.shape, var, cfg.shape->index_REC(), MT_GT_MF_GF_BT);

	if (eqsplit) {
		// check cond
		result.push_back(Cfg(cfg, eqsplit));
		bool cres = cfg.localepoch[tid] != cfg.epochsel;
		result.back().pc[tid] = cres ? nY : nN;
	}

	if (neqsplit) {
		// result unknown
		result.push_back(Cfg(cfg, new Shape(*neqsplit)));
		result.back().pc[tid] = nY;
		result.push_back(Cfg(cfg, neqsplit));
		result.back().pc[tid] = nN;
	}

	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const IncrementGlobalEpoch& stmt, unsigned short tid) {
	CHECK_STMT;
	auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	Epoch new_epoch;
	switch (cfg.globalepoch) {
		case Epoch::ZERO: new_epoch = Epoch::ONE; break;
		case Epoch::ONE: new_epoch = Epoch::TWO; break;
		case Epoch::TWO: new_epoch = Epoch::ZERO; break;
	}
	result.back().globalepoch = new_epoch;
	return result;
}
