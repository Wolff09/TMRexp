#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include "post/helpers.hpp"
#include "post/eval.hpp"
#include "post/setout.hpp"
#include "config.hpp"

using namespace tmr;

static inline bool is_observed_behind_and_not_free(const Cfg& cfg, unsigned short tid) {
	#if !REPLACE_INTERFERENCE_WITH_SUMMARY // we do not need hints when using summaries
		std::size_t fri = cfg.shape->index_FREE();
		for (std::size_t oct : {3,4}) {
			if (!cfg.shape->test(oct, fri, MT)) {
				if (!haveCommon(cfg.shape->at(oct, 5), EQ_MF_GF))
					return true;
			}
		}
	#endif
	return false;
}

static inline void fire_lp(Cfg& cfg, const Function& evt, unsigned short tid) {
	assert(cfg.inout[tid].type() != OValue::DUMMY);
	// std::cout << " -- fire lp " << evt.name() << "(" << cfg.inout[tid] << "): move from " << cfg.state;
	cfg.state = cfg.state.next(evt, cfg.inout[tid]);
	// std::cout << " to " << cfg.state << std::endl;

	if (cfg.state.is_final()) {
		if (!is_observed_behind_and_not_free(cfg, tid)) {
			std::cout << std::endl << "*********************************" << std::endl;
			std::cout << "Specification violation detected!" << std::endl << std::endl;
			std::cout << cfg << *cfg.shape << std::endl;
			throw std::runtime_error("Specification violation detected! Observer reached final state: " + cfg.state.find_final().name());
		}
	}
}


std::vector<Cfg> post_lp_input(const Cfg& cfg, const LinearizationPoint& stmt, Shape* emitter_shape, unsigned short tid) {
	// std::cout << " -- linp __in__" << std::endl;
	std::vector<Cfg> result;
	result.push_back(mk_next_config(cfg, emitter_shape, tid));
	fire_lp(result.back(), stmt.event(), tid);
	return result;
}

std::vector<Cfg> post_lp_output(const Cfg& cfg, const LinearizationPoint& stmt, Shape* emitter_shape, unsigned short tid) {
	// std::cout << " -- linp __out__" << std::endl;
	std::vector<Cfg> result;

	if (cfg.inout[tid].type() != OValue::DUMMY) {
		std::cout << "************************************" << std::endl;
		std::cout << "Multiple Linearization Events fired." << std::endl;
		std::cout << "already fired event for: " << cfg.inout[tid] << std::endl;
		std::cout << "wanted to fire: " << (stmt.has_var() ? "for data field of variable" : "empty") << std::endl;
		throw std::runtime_error("Multiple Linearization Events fired.");

	} else if (!stmt.has_var()) {
		// fire out(empty)
		result.push_back(mk_next_config(cfg, emitter_shape, tid));
		result.back().inout[tid] = OValue::Empty();
		fire_lp(result.back(), stmt.event(), tid);

	} else {
		auto var_index = mk_var_index(*emitter_shape, stmt.var(), tid);
		result = set_inout_output_value(cfg, var_index, tid, emitter_shape);

		for (Cfg& cf : result)
			fire_lp(cf, stmt.event(), tid);
	}

	return result;
}


Shape* merge_shapes(Shape* one, Shape* two) {
	std::vector<Shape*> shapes;
	if (one == NULL && two == NULL) return NULL;
	if (one != NULL) shapes.push_back(one);
	if (two != NULL) shapes.push_back(two);
	assert(shapes.size() > 0);
	return merge(shapes);
}

std::pair<Shape*, Shape*> get_emitter_and_silent_shape(const Cfg& cfg, const Condition& cond, Shape* emitter, Shape* silent, unsigned short tid);

std::pair<Shape*, Shape*> get_emitter_and_silent_shape(const Cfg& cfg, const EqNeqCondition& cond, Shape* emitter, Shape* silent, unsigned short tid) {
	if (emitter == NULL) return { NULL, silent };
	auto result = eval_eqneq(cfg, *emitter, cond.lhs(), cond.rhs(), cond.is_inverted(), tid);
	delete emitter;
	result.second = merge_shapes(result.second, silent);
	return result;
}

std::pair<Shape*, Shape*> get_emitter_and_silent_shape(const Cfg& cfg, const OracleCondition& cond, Shape* emitter, Shape* silent, unsigned short tid) {
	if (cfg.oracle[tid]) return { emitter, silent };
	else return { NULL, merge_shapes(emitter, silent) };
}

std::pair<Shape*, Shape*> get_emitter_and_silent_shape(const Cfg& cfg, const CompoundCondition& cond, Shape* emitter, Shape* silent, unsigned short tid) {
	auto lhs_result = get_emitter_and_silent_shape(cfg, cond.lhs(), emitter, silent, tid);
	auto rhs_result = get_emitter_and_silent_shape(cfg, cond.rhs(), lhs_result.first, lhs_result.second, tid);
	return rhs_result;
}

std::pair<Shape*, Shape*> get_emitter_and_silent_shape(const Cfg& cfg, const Condition& cond, Shape* emitter, Shape* silent, unsigned short tid) {
	switch (cond.type()) {
		case Condition::EQNEQ:
			return get_emitter_and_silent_shape(cfg, static_cast<const EqNeqCondition&>(cond), emitter, silent, tid);

		case Condition::COMPOUND:
			return get_emitter_and_silent_shape(cfg, static_cast<const CompoundCondition&>(cond), emitter, silent, tid);

		case Condition::ORACLEC:
			return get_emitter_and_silent_shape(cfg, static_cast<const OracleCondition&>(cond), emitter, silent, tid);

		default:
			throw std::logic_error("Unsupported condition type for linearization point.");
	}
}

std::pair<Shape*, Shape*> get_emitter_and_silent_shape(const Cfg& cfg, const LinearizationPoint& stmt, unsigned short tid) {
	if (!stmt.has_cond())
		return { new Shape(*cfg.shape), NULL };
	
	return get_emitter_and_silent_shape(cfg, stmt.cond(), new Shape(*cfg.shape), NULL, tid);
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const LinearizationPoint& stmt, unsigned short tid) {
	CHECK_STMT;

	/* For input functions, we must ensure that we emit an event with the input value __in__,
	 * and not with the value stored.
	 * 
	 * For output functions, we have to read the value of _var, if _var != NULL, and emit it,
	 * or we have to emit empty, if _var == NULL.
	 */
	
	auto espair = get_emitter_and_silent_shape(cfg, stmt, tid);
	Shape* emitter_shape = espair.first;
	Shape* silent_shape = espair.second;

	std::vector<Cfg> result;

	// delegate the work for emitting an parametrized event
	if (emitter_shape != NULL) {
		assert(stmt.event().has_input() ^ stmt.event().has_output());	
		if (stmt.event().has_input()) result = post_lp_input(cfg, stmt, emitter_shape, tid);
		else result = post_lp_output(cfg, stmt, emitter_shape, tid);
	}

	// do nothing for the silent shape
	if (silent_shape != NULL)
		result.push_back(mk_next_config(cfg, silent_shape, tid));

	return result;
}
