#include "post.hpp"

#include <stdexcept>
#include <assert.h>
#include "relset.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/setout.hpp"
#include "post/assign.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** MALLOC ********************************/

// bool free_cells_still_free(const Shape& input, const Shape& output) {
// 	// if t↦FREE in input shape, then we want to have t↦FREE in output shape, too, to check for prf
// 	for (std::size_t i = input.offset_vars(); i < input.size(); i++)
// 		if (input.test(i, input.index_FREE(), MT))
// 			if (!output.test(i, input.index_FREE(), MT))
// 				return false;
// 	return true;
// }

#define NON_LOCAL(x) !(x >= cfg.shape->offset_locals(tid) && x < cfg.shape->offset_locals(tid) + cfg.shape->sizeLocals())
; // this semicolon is actually very useful: it fixes my syntax highlighting :)

std::vector<Cfg> tmr::post(const Cfg& cfg, const Malloc& stmt, unsigned short tid) {
	CHECK_STMT;
	
	const Shape& input = *cfg.shape;
	const auto var_index = mk_var_index(input, stmt.decl(), tid);

	if (NON_LOCAL(var_index) && &stmt.function().prog().init_fun() != &stmt.function()) {
			throw std::runtime_error("Allocations may not target global variables.");
	}

	std::vector<Cfg> result;

	/* malloc gives a fresh memory cell */
	result.push_back(mk_next_config(cfg, new Shape(input), tid));
	Cfg& fresh = result.back();

	for (std::size_t i = 0; i < input.size(); i++)
		fresh.shape->set(var_index, i, BT_);
	fresh.shape->set(var_index, var_index, EQ_);
	fresh.shape->set(var_index, input.index_NULL(), MT_);
	fresh.own.set(var_index, true);
	fresh.valid_ptr.set(var_index, true);
	fresh.valid_next.set(var_index, true);
	fresh.guard0state.set(var_index, NULL);
	fresh.guard1state.set(var_index, NULL);


	/* malloc gives a freed cell */
	if (cfg.freed) {
		auto shape1 = post_assignment_pointer_shape_var_var(input, var_index, input.index_REUSE(), &stmt);
		auto shape2 = post_assignment_pointer_shape_next_var(*shape1, var_index, input.index_NULL(), &stmt); // TODO: correct?
		delete shape1;

		result.push_back(mk_next_config(cfg, shape2, tid));
		Cfg& reuse = result.back();

		reuse.own.set(var_index, true);
		reuse.valid_ptr.set(var_index, true);
		reuse.valid_next.set(var_index, true);
		reuse.guard0state.set(var_index, NULL);
		reuse.guard1state.set(var_index, NULL);

		reuse.freed = false;
		reuse.retired = false;
	}

	return result;
}


/******************************** FREE ********************************/

// static std::vector<OValue> get_possible_data(const Cfg& cfg, std::size_t var) {
// 	std::vector<OValue> result;
// 	const Shape& shape = *cfg.shape;
// 	const Observer& obs = cfg.state.observer();
// 	bool anonymous = true;
// 	for (std::size_t i = 0; i < shape.sizeObservers(); i++) {
// 		auto cell = shape.at(var, shape.index_ObserverVar(i));
// 		if (cell.test(EQ)) result.push_back(obs.mk_var(i));
// 		if (!haveCommon(cell, MT_GT_MF_GF_BT)) anonymous = false;
// 	}
// 	if (anonymous) result.push_back(OValue::Anonymous());
// 	return result;
// }

// static bool is_globally_reachable(const Shape& shape, std::size_t var) {
// 	for (auto i = shape.offset_program_vars(); i < shape.offset_locals(0); i++) {
// 		if (haveCommon(shape.at(i, var), EQ_MT_GT)) {
// 			std::cout << std::endl << "var=" << var << "; i=" << i << "; shape.at(i, var):" << shape.at(i,var) << std::endl;
// 			return true;
// 		}
// 	}
// 	return false;
// }

// std::vector<Cfg> tmr::post(const Cfg& cfg, const Free& stmt, unsigned short tid) {
// 	throw std::logic_error("Not yet implemented.");
	// TODO: this needs a complete rewrite

	// CHECK_STMT;
	// const Shape& input = *cfg.shape;

	// const auto var_index = mk_var_index(input, stmt.decl(), tid);
	// // std::cout << "free(" << var_index << ")" << std::endl;
	// CHECK_ACCESS(var_index);

	// /* with garbage collection, a free has no effect */
	// if (msetup == GC) return mk_next_config_vec(cfg, new Shape(input), tid);

	// /* without garbage collection, a free marks a part of the heap as free
	//  * but the content of the freed memory cell is not altered
	//  */
	// CHECK_PRF_ws(var_index, stmt);
	// if (input.test(var_index, input.index_FREE(), MT)) {
	// 	std::cout << "*********************" << std::endl;
	// 	std::cout << "Double Free detected!" << std::endl;
	// 	std::cout << "during: " << stmt << std::endl;
	// 	std::cout << "with: " << stmt.var() << "=" << var_index << std::endl;
	// 	std::cout << "in cfg: " << cfg << *cfg.shape << *cfg.ages << std::endl;
	// 	throw std::runtime_error("Double free: while freeing cell term with id=" + std::to_string(var_index) + ".");
	// }
	// #if REPLACE_INTERFERENCE_WITH_SUMMARY
	// 	if (is_globally_reachable(input, var_index)) {
	// 		std::cout << cfg << *cfg.shape << std::endl;
	// 		throw std::runtime_error("Globally reachable cells may not be freed.");
	// 	}
	// #endif

	// const auto free_index = input.index_FREE();
	// const std::vector<std::size_t> free_index_vec = { free_index };
	
	// std::vector<Cfg> result;
	
	// auto shapes = disambiguate(input, var_index);
	// result.reserve(shapes.size());

	// if (msetup == PRF) {
	// 	/* we remove the successors of the freed variable as no one is allowed to access
	// 	 * its next field again (guarantee due to pointer race freedom)
	// 	 * then mark the cell as being freed
	// 	 */
	// 	for (Shape* shape : shapes) {
	// 		remove_successors(*shape, var_index);

	// 		auto eqVar = get_related(*shape, var_index, EQ_);
	// 		auto preVar = get_related(*shape, var_index, MF_GF);
	// 		relate_all(*shape, eqVar, free_index_vec, MT);
	// 		relate_all(*shape, preVar, free_index_vec, GT);

	// 		assert(consistent(*shape));
	// 		assert(is_closed_under_reflexivity_and_transitivity(*shape));

	// 		result.push_back(mk_next_config(cfg, shape, tid));

	// 		// freeing a cell publishes them as they might be reallocated by someone
	// 		for (std::size_t i : eqVar) {
	// 			result.back().own.publish(i);
	// 			result.back().invalid[i] = true;
	// 		}
	// 	}

	// } else if (msetup == MM) {
	// 	/* the cell is freed but we can still access it
	// 	 * mark it free (done via age matrix), but do not alter the shape
	// 	 * (note the note below!)
	// 	 */
	// 	for (Shape* shape : shapes) {
	// 		result.push_back(mk_next_config(cfg, shape, tid));
	// 		auto eqVar = get_related(*shape, var_index, EQ_);
	// 		for (std::size_t i : eqVar) {
	// 			// result.back().own.publish(i);
	// 			result.back().ages->set(i, false, free_index, true, AgeRel::EQ);
	// 		}
	// 	}
	// }

	// if (stmt.function().has_output()) {
	// 	// make the ObsFree observer move
	// 	// TODO: we have to find the deleted data value, but we rely on the fact that the cell is freed
	// 	// which was used for firing the linearisation event
	// 	for (Cfg& cf : result) {
	// 		std::vector<OValue> possible_data = get_possible_data(cf, var_index);
	// 		if (possible_data.size() != 1) {
	// 			std::cout << cf << *cf.shape << std::endl << "values("<<possible_data.size()<<"): ";
	// 			for (auto v : possible_data) std::cout << v << " ";
	// 			std::cout << std::endl;
	// 			throw std::runtime_error("Unsupported free: multiple data values possible.");
	// 		}
	// 		OValue freed_data = possible_data.front();

	// 		cf.state = cf.state.next(stmt.function().prog().freefun(), freed_data);
	// 		if (cf.state.is_final())
	// 			throw std::runtime_error("Double free: while freeing cell containing observed value");
	// 	}
	// }

	// return result;
// }
