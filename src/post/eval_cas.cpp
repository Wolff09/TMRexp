#include "post/eval.hpp"

#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/assign.hpp"

using namespace tmr;


void increase_age(std::vector<Cfg>& splitinto, Cfg& cfg, std::size_t dst) {
	// increase dst_next by one (inplace cfg)
	// if a branch is needed, push the new configuration to splitinto
	// (cfg is already a member of splitinto)

	// find some age entry t with dst.age<t.age such that: for all t'. dst.age<t'.age => t.age=t'.age or t.age<t'.age
	// ~> t.age is smallest age bigger than dst.age
	std::size_t sup = dst;
	for (std::size_t t = 0; t < cfg.ages->size(); t++)
		if (cfg.ages->at(dst, t) == AgeRel::LT)
			if (cfg.ages->at(t, sup) == AgeRel::LT)
				sup = t;

	// increase (dst,dst_next) age by one:
	//   t < dst => t < dst
	//   t = dst => t < dst
	//   t > dst => t > dst (for t!=sup)
	//   s > dst => s = dst
	//            | s > dst (for s=sup)

	// set "t = dst => t < dst" case in-place
	// do the "s > dst => s > dst" case in-place
	// (we only need to convert = into <)
	for (std::size_t t = 0; t < cfg.ages->size(); t++)
		if (t == dst) continue;
		else if (cfg.ages->at(t, dst) == AgeRel::EQ)
			cfg.ages->set(t, dst, AgeRel::LT);
	// std::cout << "dst.age++ (1)" << std::endl << cfg.ages;

	// do the "s > dst => s = dst" in a new branch (if needed)
	// copy cfg, as we already convert = into < there)
	bool branch_needed = sup != dst;
	if (branch_needed) {
		splitinto.push_back(cfg.copy());
		set_age_equal(splitinto.back(), dst, sup);
	}
}

std::size_t counter = 0;

std::vector<Cfg> tmr::eval_cond_cas(const Cfg& cfg, const CompareAndSwap& stmt, const Statement* nextTrue, const Statement* nextFalse, unsigned short tid, MemorySetup msetup) {
	std::vector<Cfg> result;
	const Shape& input = *cfg.shape;

	// if (tid == 2 && cfg.pc[0] != NULL && cfg.pc[0]->id()==10) std::cout << "CAS for: " << cfg << *cfg.shape << *cfg.ages;
	
	auto dst = mk_var_index(input, stmt.dst(), tid);
	bool cmp_null = stmt.cmp().clazz() == Expr::NIL;
	auto cmp = cmp_null ? input.index_NULL() : mk_var_index(input, stmt.cmp(), tid);

	bool compare_age_fields = stmt.update_age_fields() && !cmp_null;
	bool update_age_fields = !cmp_null; // TODO: why? :)

	if (compare_age_fields && cfg.ages->at(dst, cmp) != AgeRel::BOT && cfg.ages->at(dst, cmp) != AgeRel::EQ) {
		// shortcut heavy weight computation if the age fields mismatch definitely
		// => comparison evaluates to false and the CAS does nothing
		result.push_back(mk_next_config(cfg, new Shape(*cfg.shape), nextFalse, tid));
		return result;
	}

	// check the shape for condition
	std::pair<Shape*, Shape*> sp = eval_eqneq(cfg, stmt.dst(), stmt.cmp(), false, tid, msetup);

	if (sp.first != NULL) {
		// condition may evaluate to true in the shape
		// check whether the age fields match

		if (compare_age_fields && cfg.ages->at(dst, cmp) == AgeRel::BOT) {
			// result.push_back(Cfg(cfg, new Shape(*sp.first)));
			// result.push_back(Cfg(cfg, new Shape(*sp.first)));
			// result.push_back(Cfg(cfg, sp.first));

			// result[0].ages->set(dst, cmp, AgeRel::LT);
			// result[1].ages->set(dst, cmp, AgeRel::EQ);
			// result[2].ages->set(dst, cmp, AgeRel::GT);

			std::cout << "An age field misuse was detected (relation is undefined) in the following CAS operation: " << std::endl << "    ";
			stmt.print(std::cout, 1);
			std::cout << std::endl << "For tid="<<tid<<" in the following configuration: " << std::endl << "    " << cfg << *cfg.shape << *cfg.ages << std::endl;
			std::cout << "While accessing ages["<<dst<<"]["<<cmp<<"]" << std::endl;
			delete sp.first;
			throw std::runtime_error("Age Field Misuse detected!");

		} else if (compare_age_fields && cfg.ages->at(dst, cmp) != AgeRel::EQ) {
			// age fields mismatch => false branch
			// TODO: this case does never apply (caught above)
			result.push_back(mk_next_config(cfg, sp.first, nextFalse, tid));

		} else {
			// compare evaluates to true
			Cfg tmp(cfg, sp.first);
			assert(!compare_age_fields || tmp.ages->at(dst, cmp) == AgeRel::EQ);

			// the CAS stmt appears either in an ITE or on its own (in the latter case, the next field is set)
			assert(tmp.pc[tid]->next() == NULL || (tmp.pc[tid]->next() == nextTrue && tmp.pc[tid]->next() == nextFalse));

			// execute: dst = src (beware, the following advances the pc and updates age fields)
			tmp = tmr::post_assignment_pointer(tmp, stmt.dst(), stmt.src(), tid, msetup, &stmt);
			assert(tmp.pc[tid] == NULL || (tmp.pc[tid] == nextTrue && tmp.pc[tid] == nextFalse));
			tmp.pc[tid] = NULL; // if CAS appears on its own, we don't want the pc to continue, otherwise it is NULL anyway

			// undo age field update: the assignment sets dst.age~src.age to = or maybe ⊥
			// but we want to update the age fields manually below
			tmp.ages.reset(new AgeMatrix(*cfg.ages)); // TODO: change api to allow assignments that do not update the age fields

			// execute: linearization point (if needed)
			if (stmt.fires_lp()) {
				tmp.pc[tid] = &stmt.lp();
				assert(stmt.lp().next() == NULL);
				result = tmr::post(tmp, stmt.lp(), tid, msetup);
			} else {
				result.push_back(std::move(tmp));
			}

			std::size_t osize = result.size();
			result.reserve(osize * 2); // be careful here!
			for (std::size_t i = 0; i < osize; i++) {
				Cfg& c = result[i];

				// pc should be a dummy value by now
				assert(c.pc[tid] == NULL);
				
				// manually set pc
				c.pc[tid] = nextTrue;

				// if (!cmp_null) {
					// set dst.age = cmp.age
					set_age_equal(c, dst, cmp);

					// execute dst.age++ (if demanded by program)
					if (update_age_fields)
						increase_age(result, c, dst);
				// }
			}
		}
	}

	if (sp.second != NULL) {
		// compare evaluates to false => just do nothing and go on
		result.push_back(mk_next_config(cfg, sp.second, nextFalse, tid));
	}

	assert(result.size() > 0);
	return result;
}
