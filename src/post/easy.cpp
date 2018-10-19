#include "post.hpp"

#include "post/helpers.hpp"
#include "post/eval.hpp"

using namespace tmr;


static inline std::vector<Cfg> noop(const Cfg& cfg, unsigned short tid) {
	assert(cfg.pc[tid] != NULL);
	std::vector<Cfg> result;
	result.push_back(cfg.copy());
	result.back().pc[tid] = cfg.pc[tid]->next();
	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const Sequence& stmt, unsigned short tid) {
	CHECK_STMT;
	return noop(cfg, tid);
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const Break& stmt, unsigned short tid) {
	CHECK_STMT;
	return noop(cfg, tid);
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const CompareAndSwap& stmt, unsigned short tid) {
	CHECK_STMT;
	return eval_cond_cas(cfg, stmt, stmt.next(), stmt.next(), tid);
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const Conditional& stmt, unsigned short tid) {
	CHECK_STMT;
	return eval_cond(cfg, stmt, tid);
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const Killer& stmt, unsigned short tid) {
	CHECK_STMT;

	Shape* shape = new Shape(*cfg.shape);
	std::size_t var_index = mk_var_index(*shape, stmt.var(), tid);

	for (std::size_t i = 0; i < shape->size(); i++)
		shape->set(var_index, i, BT_);
	shape->set(var_index, var_index, EQ_);
	shape->set(var_index, shape->index_UNDEF(), MT_);

	auto result = mk_next_config_vec(cfg, shape, tid);
	if (var_index == shape->offset_locals(tid) && shape->sizeLocals() > 0) {
		result.back().owned[tid] = false;
	}
	return result;
}
