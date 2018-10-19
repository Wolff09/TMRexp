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

	auto result = mk_next_config_vec(cfg, shape2, tid);
	if (var_index == input.offset_locals(tid) && input.sizeLocals() > 0) {
		result.back().owned[tid] = true;
	}
	return result;
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

inline void check_state(const Cfg& cfg, const MultiState new_state, unsigned short tid) {
	if (new_state.is_final()) {
		std::cout << "Specification violation detected." << std::endl << std::endl;
		std::cout << "Freeing thread: " << tid << std::endl;
		std::cout << "Old state: " << cfg.smrstate << std::endl;
		std::cout << "New state: " << new_state << std::endl;
		std::cout << "Cfg: " << cfg << *cfg.shape << std::endl;

		throw std::runtime_error("Specification violation detected");
	}
}

inline void fire_free_event(Cfg& cfg, unsigned short tid) {
	// fire smrstate event
	auto new_smrstate = cfg.smrstate.next(Event::mk_free(cfg.offender[tid], DataValue::DATA));
	check_state(cfg, new_smrstate, tid);

	// fire threadstate event for thread 0
	auto new_threadstate0 = cfg.threadstate[0].next(Event::mk_free(tid == 0, DataValue::DATA));
	check_state(cfg, new_threadstate0, tid);

	// fire threadstate event for thread 1; thread 1 is only present if tid == 1
	auto new_threadstate1 = cfg.threadstate[1];
	if (tid == 1) {
		new_threadstate1 = cfg.threadstate[1].next(Event::mk_free(tid == 1, DataValue::DATA));
		check_state(cfg, new_threadstate1, tid);
	}

	// update states last to avoid broken debug output
	cfg.smrstate = new_smrstate;
	cfg.threadstate[0] = new_threadstate0;
	cfg.threadstate[1] = new_threadstate1;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const FreeAll& stmt, unsigned short tid) {
	// ignoring free(DataValue::OTHER) due to elision support stating that the observer does not react on it
	DataSet set = getset(cfg, stmt.setid(), tid);

	switch (set) {
		case DataSet::WITHOUT_DATA:
			return mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);

		case DataSet::WITH_DATA: {
			auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
			fire_free_event(result.back(), tid);
			return result;
		}
	}
	assert(false);
}
