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

std::vector<Cfg> tmr::post(const Cfg& cfg, const Oracle& stmt, unsigned short tid) {
	CHECK_STMT;

	std::vector<Cfg> result;

	result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), tid));
	result.back().oracle[tid] = false;

	result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), tid));
	result.back().oracle[tid] = true;

	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const CheckProphecy& stmt, unsigned short tid) {
	CHECK_STMT;

	std::vector<Cfg> result;
	if (cfg.oracle[tid] == stmt.cond()) result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), tid));
	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const Killer& stmt, unsigned short tid) {
	CHECK_STMT;

	if (stmt.kill_confused()) {
		if (cfg.inout[tid].type() != OValue::DUMMY) return {};
		else return mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	}

	Shape* shape = new Shape(*cfg.shape);
	std::size_t var_index = mk_var_index(*shape, stmt.var(), tid);

	for (std::size_t i = 0; i < shape->size(); i++)
		shape->set(var_index, i, BT_);
	shape->set(var_index, var_index, EQ_);
	shape->set(var_index, shape->index_UNDEF(), MT_);

	std::vector<Cfg> result;
	result.push_back(mk_next_config(cfg, shape, tid));
	Cfg& back = result.back();

	back.own.set(var_index, true);
	back.valid_ptr.set(var_index, false);
	back.valid_next.set(var_index, false);
	back.guard0state.set(var_index, nullptr);
	back.guard1state.set(var_index, nullptr);


	return result;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const EnforceReach& stmt, unsigned short tid) {
	CHECK_STMT;

	const Shape& shape = *cfg.shape;
	std::size_t var_index = mk_var_index(shape, stmt.var(), tid);
	std::vector<Shape*> shapes = disambiguate(shape, var_index);

	for (Shape* s : shapes) {
		if (s == NULL) continue;

		bool reachable = false;
		for (std::size_t i = cfg.shape->offset_program_vars(); i < cfg.shape->offset_locals(0); i++) {
			if (haveCommon(s->at(i, var_index), EQ_MT_GT) || s->test(var_index, s->index_NULL(), EQ)) {
				reachable = true;
			}
		}
		if (!reachable) {
			std::cout << stmt.var() << " (id=" << var_index << ") is not reachable from share variables as requested." << std::endl;
			std::cout << cfg << *cfg.shape << std::endl;
			throw std::runtime_error("Requested reachability could not be established.");
		}

		delete s;
	}

	return mk_next_config_vec(cfg, new Shape(shape), tid);
}
