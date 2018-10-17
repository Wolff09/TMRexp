#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include <deque>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/assign.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** MALLOC ********************************/

#define NON_LOCAL(x) !(x >= cfg.shape->offset_locals(tid) && x < cfg.shape->offset_locals(tid) + cfg.shape->sizeLocals())
; // this semicolon is actually very useful: it fixes my syntax highlighting :)

std::vector<Cfg> tmr::post(const Cfg& cfg, const Malloc& stmt, unsigned short tid) {
	CHECK_STMT;
	
	const Shape& input = *cfg.shape;
	const auto var_index = mk_var_index(input, stmt.decl(), tid);

	/* malloc gives a fresh memory cell */
	auto shape1 = new Shape(input);
	for (std::size_t i = 0; i < input.size(); i++)
		shape1->set(var_index, i, BT_);
	shape1->set(var_index, var_index, EQ_);
	auto shape2 = post_assignment_pointer_shape_next_var(*shape1, var_index, input.index_NULL(), &stmt); // null init
	delete shape1;

	return mk_next_config_vec(cfg, shape2, tid);
}


/******************************** FREE ********************************/

inline DataSet getset(const Cfg& cfg, std::size_t setid, unsigned short tid) {
	switch (setid) {
		case 0: return cfg.dataset0[tid];
		case 1: return cfg.dataset1[tid];
		case 2: return cfg.dataset2[tid];
		default: throw std::logic_error("Unsupported dataset id.");
	}
}

inline void fire_free_event(Cfg& cfg) {
	Event evt = Event::mk_free(DataValue::DATA);
	cfg.state = cfg.state.next(evt);

	// check for final state
	if (cfg.state.is_final()) {
		throw std::runtime_error("Specification violation detected");
	}
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const FreeAll& stmt, unsigned short tid) {
	// ignoring free(DataValue::OTHER) due to elision support stating that the observer does not react on it
	DataSet set = getset(cfg, stmt.setid(), tid);

	switch (set) {
		case DataSet::WITHOUT_DATA:
			return mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);

		case DataSet::WITH_DATA: {
			auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
			fire_free_event(result.back());
			return result;
		}
	}
	assert(false);
}
