#include "post/eval.hpp"

#include "config.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/assign.hpp"
#include <deque>
#include <stack>

using namespace tmr;


// static inline AgeRel mk_increased(AgeRel rel, bool increase_age) {
// 	if (increase_age && rel == AgeRel::EQ) return AgeRel::GT;
// 	else return rel;
// }

// static inline std::pair<std::size_t, bool> get_next_bigger(const AgeMatrix& ages, std::size_t cmp, bool cmpSel) {
// 	#if SIMULATE_BUGGY_BEHAVIOUR_FROM_PRF_PAPER
// 		return {cmp, cmpSel};
// 	#endif

// 	std::size_t res;
// 	bool resSel;

// 	// find initial age that is larger
// 	for (std::size_t i = 0; i < ages.size(); i++) {
// 		for (bool b : {false, true}) {
// 			if (ages.at(cmp, cmpSel, i, b) == AgeRel::LT) {
// 				res = i;
// 				resSel = b;
// 				goto search_smaller;
// 			}
// 		}
// 	}

// 	return { cmp, cmpSel };

// 	search_smaller:
// 	/* in case we found something larager,
// 	 * we want to find the smallest one that is larger than cmp/cmpSel
// 	 */

// 	auto start = res;
// 	for (std::size_t i = start; i < ages.size(); i++) {
// 		for (bool b : {false, true}) {
// 			if (ages.at(cmp, cmpSel, i, b) == AgeRel::LT) {
// 				if (ages.at(i, b, res, resSel) == AgeRel::LT) {
// 					res = i;
// 					resSel = b;
// 				}
// 			}
// 		}
// 	}

// 	return { res, resSel };
// }

// static inline Cfg* update_age(std::deque<Cfg>& split, Cfg& cfg, std::size_t dst, bool dstSel, std::size_t /*cmp*/, bool /*cmpSel*/, bool increase_age) {
// 	// update all relations
// 	for (std::size_t i = 0; i < cfg.ages->size(); i++) {
// 		for (bool b : {false, true}) {
// 			if (i == dst && b == dstSel) continue;
// 			auto rel = cfg.ages->at(dst, dstSel, i, b);
// 			auto new_rel = mk_increased(rel, increase_age);
// 			cfg.ages->set(dst, dstSel, i, b, new_rel);
// 		}
// 	}

// 	// if the age field should not be increased we are done
// 	if (!increase_age) {
// 		return NULL;
// 	}

// 	auto bound = get_next_bigger(*cfg.ages, dst, dstSel);
// 	std::size_t sup = bound.first;
// 	bool supSel = bound.second;

// 	// check if a split is required
// 	// this happens if there is some larger age field: it could be equal now
// 	Cfg* splitted_cfg = NULL;
// 	if (sup != dst || supSel != dstSel) {
// 		// we need to split
// 		split.push_back(cfg.copy());
// 		splitted_cfg = &split.back();
		

// 		// it doesn't matter which cfg we update -> do it inplace with the original one, not the copy
// 		// the age field may now coincide with some previouly larger one
// 		// make dst/dstSel equal to sup/supSel -> copy the row
// 		for (std::size_t i = 0; i < cfg.ages->size(); i++)
// 			for (bool b : {false, true})
// 				cfg.ages->set(dst, dstSel, i, b, cfg.ages->at(sup, supSel, i, b));
// 		cfg.ages->set(dst, dstSel, sup, supSel, AgeRel::EQ);
// 	}

// 	return splitted_cfg;
// }

// static inline void mk_next_age_equal(Cfg& cfg, std::size_t dst, std::size_t src, bool srcSel) {
// 	cfg.ages->set(src, srcSel, dst, true, AgeRel::EQ);
// 	for (std::size_t i = 0; i < cfg.shape->size(); i++)
// 		for (bool b : {false, true})
// 			cfg.ages->set(dst, true, i, b, cfg.ages->at(src, srcSel, i, b));
// }

// static inline void propagate_age_update(Cfg& conf, std::deque<Cfg>& tmpres, std::size_t dst) {
// 	// here we propagate an age field update to all pointers that are equal
// 	// and thus experience the update too => only for age updates of next fields

// 	// split to find truly equal pointers
// 	auto shape_split = disambiguate(*conf.shape, dst);

// 	for (Shape* s : shape_split) {
// 		tmpres.emplace_back(Cfg(conf, s));
// 		Cfg& config = tmpres.back();

// 		// pointer equal to dst => experiences age update too
// 		for (std::size_t i = 0; i < s->size(); i++) {
// 			if (i == dst) continue;
// 			if (s->test(i, dst, EQ)) {
// 				#if CAS_OVERAPPROXIMATE_AGE_PROPAGATION
// 					// overapproximation: drop age relation of pointers that observe the age assignemnt
// 					for (std::size_t j = 0; j < s->size(); j++)
// 						for (bool b : {false, true})
// 							config.ages->set(j, b, i, true, AgeRel::BOT);
// 				#else
// 					mk_next_age_equal(config, i, dst, true);
// 				#endif
// 			}
// 		}
// 	}

// 	// the shape/ages from conf may no longer be valid => overwrite the shape/ages
// 	conf.shape = std::move(tmpres.back().shape);
// 	conf.ages = std::move(tmpres.back().ages);
// 	tmpres.pop_back();
// }

std::vector<Cfg> tmr::eval_cond_cas(const Cfg& cfg, const CompareAndSwap& stmt, const Statement* nextTrue, const Statement* nextFalse, unsigned short tid) {
	// const Shape& input = *cfg.shape;
	std::vector<Cfg> result;
	
	// auto dst = mk_var_index(input, stmt.dst(), tid);
	// auto src = mk_var_index(input, stmt.src(), tid);
	// bool cmp_null = stmt.cmp().clazz() == Expr::NIL;
	// auto cmp = cmp_null ? input.index_NULL() : mk_var_index(input, stmt.cmp(), tid);

	// check the shape for condition
	std::pair<Shape*, Shape*> sp = eval_eqneq(cfg, stmt.dst(), stmt.cmp(), false, tid);

	if (sp.first != NULL) {
		// compare evaluates to true
		Cfg tmp(cfg, sp.first);

		// the CAS stmt appears either in an ITE or on its own (in the latter case, the next field is set)
		assert(tmp.pc[tid]->next() == NULL || (tmp.pc[tid]->next() == nextTrue && tmp.pc[tid]->next() == nextFalse));

		// execute: dst = src (beware, the following advances the pc)
		tmp = tmr::post_assignment_pointer(tmp, stmt.dst(), stmt.src(), tid, &stmt);
		assert(tmp.pc[tid] == NULL || (tmp.pc[tid] == nextTrue && tmp.pc[tid] == nextFalse));
		tmp.pc[tid] = NULL; // if CAS appears on its own, we don't want the pc to continue, otherwise it is NULL anyway

		// execute: linearization point (if needed)
		if (stmt.fires_lp()) {
			tmp.pc[tid] = &stmt.lp();
			assert(stmt.lp().next() == NULL);
			result = tmr::post(tmp, stmt.lp(), tid);
		} else {
			result.push_back(std::move(tmp));
		}

		for (Cfg& rescfg : result) {
			rescfg.pc[tid] = nextTrue;
		}

		// update ages
		// std::deque<Cfg> tmpres;
		// for (Cfg& precfg : result) {
		// 	precfg.pc[tid] = nextTrue;

		// 	if (!update_age_fields)
		// 		continue;

		// 	// execute age field update
		// 	#if CAS_OVERAPPROXIMATE_AGE_ASSIGNMENT
		// 		// overapproximation: everything bigger does not matter, but smaller as it might simulates LL/SC
		// 		for (std::size_t i = 0; i < precfg.shape->size(); i++)
		// 			for (bool b : {false, true}) {
		// 				AgeRel newrel = mk_increased(precfg.ages->at(dst, ageT, i, b), true);
		// 				if (newrel == AgeRel::LT) newrel = AgeRel::BOT;
		// 				precfg.ages->set(dst, ageT, i, b, newrel);
		// 			}
		// 		precfg.ages->set(dst, ageT, dst, ageT, AgeRel::EQ);
		// 		precfg.ages->set(dst, ageT, cmp, false, AgeRel::GT);
		// 	#else
		// 		Cfg* split = update_age(tmpres, precfg, dst, ageT, cmp, false, update_age_fields);
		// 	#endif

		// 	// create an iterable containing those configurations we have to handle
		// 	std::deque<std::reference_wrapper<Cfg>> updates;
		// 	updates.push_back(precfg);
		// 	#if !CAS_OVERAPPROXIMATE_AGE_ASSIGNMENT
		// 		if (split) updates.push_back(*split);
		// 	#endif

		// 	// if the CAS modifies some pointer, this implicitly changes the next age field, too
		// 	// so copy the next field from the source to the destination
		// 	if (!ageT) {
		// 		mk_next_age_equal(precfg, dst, src, true);
		// 		#if !CAS_OVERAPPROXIMATE_AGE_ASSIGNMENT
		// 			if (split) mk_next_age_equal(*split, dst, src, true);
		// 		#endif
		// 	}

		// 	// changing the age of a next field is an update to the heap and thus shared with
		// 	// everyone holding a pointer to that cell => spread knowledge
		// 	if (ageT) {
		// 		propagate_age_update(precfg, tmpres, dst);
		// 		#if !CAS_OVERAPPROXIMATE_AGE_ASSIGNMENT
		// 			if (split) propagate_age_update(*split, tmpres, dst);
		// 		#endif
		// 	}
		// }

		// result.reserve(result.size() + tmpres.size());
		// for (Cfg& mv : tmpres) result.push_back(std::move(mv));
	}

	if (sp.second != NULL) {
		// compare evaluates to false => just do nothing and go on
		result.push_back(mk_next_config(cfg, sp.second, nextFalse, tid));
	}

	return result;
}
