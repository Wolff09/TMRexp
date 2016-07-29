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

bool free_cells_still_free(const Shape& input, const Shape& output) {
	// if t↦FREE in input shape, then we want to have t↦FREE in output shape, too, to check for prf
	for (std::size_t i = input.offset_vars(); i < input.size(); i++)
		if (input.test(i, input.index_FREE(), MT))
			if (!output.test(i, input.index_FREE(), MT))
				return false;
	return true;
}

#define NON_LOCAL(x) !(x >= cfg.shape->offset_locals(tid) && x < cfg.shape->offset_locals(tid) + cfg.shape->sizeLocals())
; // this one is actually very useful: it fixes my syntax highlighting :)

std::vector<Cfg> tmr::post(const Cfg& cfg, const Malloc& stmt, unsigned short tid, MemorySetup msetup) {
	CHECK_STMT;
	const Shape& input = *cfg.shape;

	std::vector<Cfg> result;
	const auto free_index = input.index_FREE();
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
	for (std::size_t i = 0; i < input.size(); i++)
		result.back().shape->set(var_index, i, BT_);
	result.back().shape->set(var_index, var_index, EQ_);
	set_age_equal(result.back(), var_index, input.index_NULL());
	if (msetup != MM) {
		result.back().own.own(var_index);
		result.back().sin[var_index] = false;
		result.back().invalid[var_index] = false;
	}

	/* with GC a free cell is guaranteed to be fresh (no one has a pointer to it)
	 * without GC the cell can still be pointed to
	 * (seperate this step from the above to avoid overwriting relations that are added due to transitivity)
	 */
	if (msetup == PRF) {
		#if PRF_REALLOCATION
			for (std::size_t i = input.offset_vars(); i < input.size(); i++) { // TODO: i = offset_program_vars ?
				if (i == var_index) continue;
				
				Shape* split = isolate_partial_concretisation(input, i, free_index, MT_);
				if (split == NULL) continue;

				auto eqI = get_related(*split, i, EQ_);
				auto preI = get_related(*split, i, MF_GF);

				// the cell pointed to by all cells in eqI is reallocated for var => var = eqI and eqI no longer free
				for (auto eqi : eqI) {
					split->set(eqi, free_index, GT_); // equal-to-i cell reallocated (use GT since next fields have to remain invalid)
					split->set(eqi, var_index, EQ_); // equal-to-i cell reallocated for var
				}

				// all predecessors of eqI can no longer reach FREE via eqI
				for (auto prei : preI)
					if (!consistent(*split, prei, free_index, GT)) {
						split->remove_relation(prei, free_index, GT);
						if (split->at(prei, free_index).none())
							split->set(prei, free_index, BT_);
					}

				// var is now equal to i
				for (std::size_t j = 0; j < split->size(); j++)
					split->set(var_index, j, split->at(i, j));

				result.push_back(mk_next_config(cfg, split, tid));
				set_age_equal(result.back(), var_index, i);
				result.back().own.own(var_index); // TODO: we may own more
				result.back().sin[var_index] = false; // TODO: there may be less sins
				result.back().invalid[var_index] = false;
			}
		#endif
	} else if (msetup == MM) {
		for (std::size_t i = input.offset_vars(); i < input.size(); i++) {
			if (i == var_index) continue;
			if (cfg.ages->at(i, false, free_index, true) != AgeRel::EQ) continue;
			
			auto shapes = disambiguate(*cfg.shape, i);
			for (Shape* split : shapes) {
				if (split->at(i, split->index_UNDEF()) != BT_) {
					delete split;
					continue;
				}

				auto eqI = get_related(*split, i, EQ_);
				auto preI = get_related(*split, i, MF_GF);
				// the cell pointed to by all cells in eqI is reallocated for var => var = eqI and eqI no longer free
				for (auto eqi : eqI) {
					split->set(eqi, free_index, BT_); // equal-to-i cell reallocated
					split->set(eqi, var_index, EQ_); // equal-to-i cell reallocated for var
				}
				// all predecessors of eqI can no longer reach FREE via eqI
				for (auto prei : preI)
					if (!consistent(*split, prei, free_index, GT)) {
						split->remove_relation(prei, free_index, GT);
						if (split->at(prei, free_index).none())
							split->set(prei, free_index, BT_);
					}
				// var is now equal to i
				for (std::size_t j = 0; j < split->size(); j++)
					split->set(var_index, j, split->at(i, j));

				result.push_back(mk_next_config(cfg, split, tid));
				set_age_equal(result.back(), var_index, i);
				for (auto i : eqI)
					result.back().ages->set(i, true, free_index, false, AgeRel::BOT); // TODO: GT instead of BOT?
				result.back().ages->set(var_index, true, free_index, false, AgeRel::BOT); // TODO: GT instead of BOT?
			}
		}
	}

	return result;
}


/******************************** FREE ********************************/

static std::vector<OValue> get_possible_data(const Cfg& cfg, std::size_t var) {
	std::vector<OValue> result;
	const Shape& shape = *cfg.shape;
	const Observer& obs = cfg.state.observer();
	bool anonymous = true;
	for (std::size_t i = 0; i < shape.sizeObservers(); i++) {
		auto cell = shape.at(var, shape.index_ObserverVar(i));
		if (cell.test(EQ)) result.push_back(obs.mk_var(i));
		if (!haveCommon(cell, MT_GT_MF_GF_BT)) anonymous = false;
	}
	if (anonymous) result.push_back(OValue::Anonymous());
	return result;
}

static bool is_globally_reachable(const Shape& shape, std::size_t var) {
	for (auto i = shape.offset_program_vars(); i < shape.offset_locals(0); i++) {
		if (haveCommon(shape.at(i, var), EQ_MT_GT)) {
			return true;
		}
	}
	return false;
}

std::vector<Cfg> tmr::post(const Cfg& cfg, const Free& stmt, unsigned short tid, MemorySetup msetup) {
	CHECK_STMT;
	const Shape& input = *cfg.shape;

	const auto var_index = mk_var_index(input, stmt.decl(), tid);
	// std::cout << "free(" << var_index << ")" << std::endl;
	CHECK_ACCESS(var_index);

	/* with garbage collection, a free has no effect */
	if (msetup == GC) return mk_next_config_vec(cfg, new Shape(input), tid);

	/* without garbage collection, a free marks a part of the heap as free
	 * but the content of the freed memory cell is not altered
	 */
	CHECK_PRF_ws(var_index, stmt);
	if (input.test(var_index, input.index_FREE(), MT)) {
		std::cout << "*********************" << std::endl;
		std::cout << "Double Free detected!" << std::endl;
		std::cout << "during: " << stmt << std::endl;
		std::cout << "with: " << stmt.var() << "=" << var_index << std::endl;
		std::cout << "in cfg: " << cfg << *cfg.shape << *cfg.ages << std::endl;
		throw std::runtime_error("Double free: while freeing cell term with id=" + std::to_string(var_index) + ".");
	}
	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		if (is_globally_reachable(input, var_index)) {
			throw std::runtime_error("Globally reachable cells may not be freed.");
		}
	#endif

	const auto free_index = input.index_FREE();
	const std::vector<std::size_t> free_index_vec = { free_index };
	
	std::vector<Cfg> result;
	
	auto shapes = disambiguate(input, var_index);
	result.reserve(shapes.size());

	if (msetup == PRF) {
		/* we remove the successors of the freed variable as no one is allowed to access
		 * its next field again (guarantee due to pointer race freedom)
		 * then mark the cell as being freed
		 */
		for (Shape* shape : shapes) {
			remove_successors(*shape, var_index);

			auto eqVar = get_related(*shape, var_index, EQ_);
			auto preVar = get_related(*shape, var_index, MF_GF);
			relate_all(*shape, eqVar, free_index_vec, MT);
			relate_all(*shape, preVar, free_index_vec, GT);

			assert(consistent(*shape));
			assert(is_closed_under_reflexivity_and_transitivity(*shape));

			result.push_back(mk_next_config(cfg, shape, tid));

			// freeing a cell publishes them as they might be reallocated by someone
			for (std::size_t i : eqVar) {
				result.back().own.publish(i);
				result.back().invalid[i] = true;
			}
		}

	} else if (msetup == MM) {
		/* the cell is freed but we can still access it
		 * mark it free (done via age matrix), but do not alter the shape
		 * (note the note below!)
		 */
		for (Shape* shape : shapes) {
			result.push_back(mk_next_config(cfg, shape, tid));
			auto eqVar = get_related(*shape, var_index, EQ_);
			for (std::size_t i : eqVar) {
				// result.back().own.publish(i);
				result.back().ages->set(i, false, free_index, true, AgeRel::EQ);
			}
		}
	}

	if (stmt.function().has_output()) {
		// make the ObsFree observer move
		// TODO: we have to find the deleted data value, but we rely on the fact that the cell is freed which was used for firing the linearisation event
		for (Cfg& cf : result) {
			std::vector<OValue> possible_data = get_possible_data(cf, var_index);
			if (possible_data.size() != 1) {
				std::cout << cf << *cf.shape << std::endl << "values("<<possible_data.size()<<"): ";
				for (auto v : possible_data) std::cout << v << " ";
				std::cout << std::endl;
				throw std::runtime_error("Unsupported free: multiple data values possible.");
			}
			OValue freed_data = possible_data.front();

			cf.state = cf.state.next(stmt.function().prog().freefun(), freed_data);
			if (cf.state.is_final())
				throw std::runtime_error("Double free: while freeing cell containing observed value");
		}
	}

	return result;
}
