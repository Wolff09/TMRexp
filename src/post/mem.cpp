#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/setout.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** MALLOC ********************************/

#define NON_LOCAL(x) !(x >= cfg.shape->offset_locals(tid) && x < cfg.shape->offset_locals(tid) + cfg.shape->sizeLocals())
; // this one is actually very useful: it fixes my syntax highlighting :)

std::vector<Cfg> tmr::post(const Cfg& cfg, const Malloc& stmt, unsigned short tid) {
	CHECK_STMT;
	const Shape& input = *cfg.shape;

	std::vector<Cfg> result;
	const auto var_index = mk_var_index(input, stmt.decl(), tid);

	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		// the first global vairable is an auxiliary one which is local to summaries
		if (NON_LOCAL(var_index) && &stmt.function().prog().init_fun() != &stmt.function()) {
			throw std::runtime_error("Allocations may not target global variables.");
		}
	#endif

	/* malloc(x) does not change the setup of the heap as it is, but simply allocates some part for x;
	 * that is, we do not need to change the shape: the cell x points to remains and the reachability via this cell is not modified;
	 * the change is: x now points to another cell
	 * 
	 * for garbage collection:  malloc(x) gives a fresh cell, i.e. one that is not reachable by any other variable/pointer/cellterm
	 * for memory management:   malloc(x) may give a cell that is pointed to by a variable/pointer/cellterm that was freed
	 */

	// malloc gives a free memory cell
	result.push_back(mk_next_config(cfg, new Shape(input), tid));
	for (std::size_t i = 0; i < input.size(); i++) {
		result.back().shape->set(var_index, i, BT_);
	}
	result.back().shape->set(var_index, var_index, EQ_);
	result.back().own.own(var_index);

	return result;
}


/******************************** FREE ********************************/

std::vector<Cfg> tmr::post(const Cfg& cfg, const Free& stmt, unsigned short tid) {
	CHECK_STMT;
	const Shape& input = *cfg.shape;

	const auto var_index = mk_var_index(input, stmt.decl(), tid);
	// std::cout << "free(" << var_index << ")" << std::endl;
	CHECK_ACCESS(var_index);

	/* with garbage collection, a free has no effect */
	return mk_next_config_vec(cfg, new Shape(input), tid);
}
