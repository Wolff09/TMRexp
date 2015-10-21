#include "fixp/interference.hpp"

#include <stdexcept>
#include "helpers.hpp"
#include "counter.hpp"

using namespace tmr;

#define SKIP_NOOPS true
#define KILL_IS_NOOP true
#define INTERFERENCE_OPTIMIZATION true

/******************************** CHECK MATCH ********************************/

bool do_ages_match(const Cfg& cfg, const Cfg& interferer, MemorySetup msetup) {
	std::size_t first_local = cfg.shape->offset_locals(msetup == MM ? 1 : 0);
	for (std::size_t row = 0; row < first_local; row++)
		for (std::size_t col = row+1; col < first_local; col++)
			for (bool br : {false, true})
				for (bool bc : {false, true})
					if (cfg.ages->at(row, br, col, bc) != interferer.ages->at(row, br, col, bc))
						return false;
	return true;
}

bool is_observed_owned(const Cfg& cfg, std::size_t obs) {
	// only for non-MM!
	std::size_t begin = cfg.shape->offset_locals(0);
	std::size_t end = begin + cfg.shape->sizeLocals();
	for (std::size_t i = begin; i < end; i++)
		if (cfg.own.is_owned(i) && cfg.shape->test(obs, i, EQ))
			return true;
	return false;
}

bool is_observed_global(const Cfg& cfg, std::size_t obs) {
	// only for non-MM!
	for (std::size_t j = 5; j < cfg.shape->offset_locals(0); j++)
		// if (intersection(cfg.shape->at(j, obs), EQ_MT_GT).any())
		if (intersection(cfg.shape->at(obs, j), MT_GT_BT).none())
			return true;
	return false;
}

bool do_shapes_match(const Cfg& cfg, const Cfg& interferer, MemorySetup msetup) {
	std::size_t end = msetup == MM ? cfg.shape->offset_locals(1) : cfg.shape->offset_locals(0);

	for (std::size_t i = 0; i < end; i++) {
		if (i == 3 || i == 4) continue;
		for (std::size_t j = i+1; j < end; j++) {
			if (j == 3 || j == 4) continue;
			if (intersection(cfg.shape->at(i, j), interferer.shape->at(i, j)).none())
			// if (!subset(cfg.shape->at(i, j), interferer.shape->at(i, j)))
			// if (cfg.shape->at(i, j) != interferer.shape->at(i, j))
				return false;
		}
	}

	for (std::size_t i : {3, 4}) {
		if (msetup != MM) {
			bool is_owned = is_observed_owned(cfg, i) || is_observed_owned(interferer, i);
			if (is_owned) continue;

			bool is_global = is_observed_global(cfg, i) || is_observed_global(interferer, i);
			if (!is_global) continue;

			for (std::size_t j = 0; j < end; j++)
				if (intersection(cfg.shape->at(i, j), interferer.shape->at(i, j)).none())
					return false;
		} else {
			for (std::size_t j = 0; j < end; j++)
				if (intersection(cfg.shape->at(i, j), interferer.shape->at(i, j)).none())
				// if (!subset(cfg.shape->at(i, j), interferer.shape->at(i, j)))
				// if (cfg.shape->at(i, j) != interferer.shape->at(i, j))
					return false;
		}
	}

	return true;
}

static bool is_noop(const Statement& pc) {
	switch (pc.clazz()) {
		case Statement::SQZ:     return true;
		case Statement::OUTPUT:  return true;
		case Statement::BREAK:   return true;
		case Statement::ORACLE:  return true;
		case Statement::CHECKP:  return true;
		#if KILL_IS_NOOP
			case Statement::KILL:    return true;
		#endif
		default: return false;
	}
}

static bool can_skip(const Cfg& cfg) {
	#define SKIP {INTERFERENCE_SKIPPED++; return true;}
	#define NO_SKIP {return false;}
	
	const Statement& pc = *cfg.pc[0];
	std::size_t tmp;

	auto set_tmp = [&] (const Expr& expr) {
		tmp = mk_var_index(*cfg.shape, expr, 0);
	};

	switch (pc.clazz()) {
		case Statement::ITE:
			if (static_cast<const Ite&>(pc).cond().type() != Condition::CASC) SKIP;
			NO_SKIP;
		case Statement::MALLOC:
			set_tmp(static_cast<const Malloc&>(pc).var());
			// malloc means redirecting pointers
			if (tmp >= cfg.shape->offset_locals(0)) SKIP; // we are not interested in local redirections
			NO_SKIP; // ...but want to see redirecting globals...
		case Statement::CAS:
			set_tmp(static_cast<const CompareAndSwap&>(pc).dst());
			break;
		case Statement::ASSIGN:
			set_tmp(static_cast<const Assignment&>(pc).lhs());
			break;
		case Statement::INPUT:
			set_tmp(static_cast<const ReadInputAssignment&>(pc).expr());
			break;
		case Statement::SETNULL:
			set_tmp(static_cast<const NullAssignment&>(pc).lhs());
			break;
		case Statement::FREE:
			set_tmp(static_cast<const Free&>(pc).var());
			break;
		default:
			NO_SKIP;
	}

	if (cfg.own.is_owned(tmp)) SKIP;
	NO_SKIP;
}

bool can_interfere(const Cfg& cfg, const Cfg& interferer, MemorySetup msetup) {
	// check whether 'interferer' can interfere with cfg
	unsigned short interferer_tid = msetup == MM ? 1 : 0;

	assert(cfg.shape);
	assert(interferer.shape);
	assert(cfg.shape->size() == interferer.shape->size());

	#if SKIP_NOOPS
		if (is_noop(*cfg.pc[0])) return false;
		if (is_noop(*interferer.pc[0])) return false;
		if (msetup == MM) {
			if (is_noop(*cfg.pc[1])) return false;
			if (is_noop(*interferer.pc[1])) return false;
		} 
	#endif

	#if INTERFERENCE_OPTIMIZATION
		if (msetup != MM) {
			if (can_skip(cfg)) return false;
			if (can_skip(interferer)) return false;
		}
	#endif

	// 1. interfering thread is executing something else, ignore pc
	//    If the pc is NULL then there will be configs which already entered some function hence we can ignore it.
	//    More precisely: it doesn't matter whether the interference happens before the first statement of a function
	//    or before calling the function (one is sufficient since calling a function does not alter the shape)
	//    => already done in tmr::mk_all_interference
	if (msetup == MM)
		if (cfg.pc[0] != interferer.pc[0])
			return false;

	// 2. cfg.state must match interferer.state
	//    => already done in tmr::mk_all_interference
	if (!(cfg.state == interferer.state))
		return false;

	// 3. prevent some data combinations which are excluded by the data independence argument
	if (msetup != MM) {
		if (cfg.inout[0] == interferer.inout[0])
			if (cfg.inout[0].type() != OValue::DUMMY)
					return false;
	} else {
		if (!(cfg.inout[0] == interferer.inout[0]))
			return false;
		if (cfg.inout[1] == interferer.inout[1])
			if (cfg.inout[1].type() != OValue::DUMMY)
				return false;
		if (cfg.inout[0] == interferer.inout[1])
			if (cfg.inout[0].type() != OValue::DUMMY)
				return false;
	}

	// 4. interfering thread should not be behind or ahead (too far) of seen
	//    A thread can be ahead of the values which have been added and removed from the data structure
	//    if they are about to add one.
	//    So we just ensure that two "out"-threads have seen the same values.
	//    For everything else, we rely on the later shape comparison (if a value was seen
	//    it is not in relation with UNDEF, otherwise it is).
	if (msetup != MM) {
		if (cfg.seen != interferer.seen && !interferer.pc[interferer_tid]->function().has_input())
		// if (cfg.seen != interferer.seen && !cfg.pc[0]->function().has_input() && !interferer.pc[interferer_tid]->function().has_input())
			return false;
	} else {
		// if (cfg.seen != interferer.seen)
		if (cfg.seen != interferer.seen && !interferer.pc[interferer_tid]->function().has_input())
		// if (cfg.seen != interferer.seen && !cfg.pc[interferer_tid]->function().has_input() && !interferer.pc[interferer_tid]->function().has_input())
			return false;
	}

	// 5. global sin must match
	for (std::size_t i = 0; i < cfg.shape->offset_locals(interferer_tid); i++)
		if (cfg.sin[i] != interferer.sin[i])
			return false;

	// 6. cfg.ages must match interferer.ages on non-local variables
	//    => already done in tmr::mk_all_interference
	if (!do_ages_match(cfg, interferer, msetup))
		return false;

	// 7. cfg.shape must be a superset of interferer.shape on the non-local cell terms
	if (!do_shapes_match(cfg, interferer, msetup))
		return false;

	// 8. ignore oracle as it is thread local

	return true;
}


/******************************** EXTENSION ********************************/

AgeRel mk_trans_rel(const AgeMatrix& ages, std::size_t row, bool row_next, std::size_t via, bool via_next, std::size_t col, bool col_next) {
	AgeRel row_via = ages.at(row, row_next, via, via_next);
	AgeRel via_col = ages.at(via, via_next, col, col_next);
	switch (row_via) {
		case AgeRel::LT:
			switch (via_col) {
				case AgeRel::LT: return AgeRel::LT;
				case AgeRel::GT: return AgeRel::BOT;
				case AgeRel::EQ: return AgeRel::LT;
				case AgeRel::BOT: return AgeRel::BOT;
			}
		case AgeRel::GT:
			switch (via_col) {
				case AgeRel::LT: return AgeRel::BOT;
				case AgeRel::GT: return AgeRel::GT;
				case AgeRel::EQ: return AgeRel::GT;
				case AgeRel::BOT: return AgeRel::BOT;
			}
		case AgeRel::EQ:
			switch (via_col) {
				case AgeRel::LT: return AgeRel::LT;
				case AgeRel::GT: return AgeRel::GT;
				case AgeRel::EQ: return AgeRel::EQ;
				case AgeRel::BOT: return AgeRel::BOT;
			}
		case AgeRel::BOT:
			return AgeRel::BOT;
	}
}

bool is_reachable(const Shape& shape, std::size_t cid) {
	// this is only used for MM
	for (std::size_t i = shape.offset_vars(); i < shape.offset_locals(1); i++)
		if (haveCommon(shape.at(i, cid), EQ_MT_GT))
			return false;
	return true;
}

Cfg* extend_cfg(const Cfg& dst, const Cfg& interferer, unsigned short extended_tid, MemorySetup msetup) {
	// extend dst with interferer (create a copy of dst, then extend the copy)
	assert(dst.shape->size() == interferer.shape->size());
	assert(dst.shape->offset_locals(0) == interferer.shape->offset_locals(0));
	assert(dst.shape->sizeLocals() == interferer.shape->sizeLocals());
	unsigned short interferer_tid = msetup == MM ? 1 : 0;

	// 1.0 extend shape
	Shape* shape = new Shape(*dst.shape);	
	shape->extend();

	// some cells can be intersected in advance, have to be unified
	std::size_t end = msetup == MM ? dst.shape->offset_locals(1) : dst.shape->offset_locals(0);
	for (std::size_t i = 0; i < end; i++) {
		if (i == 3 || i == 4) continue;
		for (std::size_t j = i+1; j < end; j++) {
			if (j == 3 || j == 4) continue;
			shape->set(i, j, intersection(dst.shape->at(i, j), interferer.shape->at(i, j)));
		}
	}
	for (std::size_t i : {3, 4}) {
		bool is_global = is_observed_global(dst, i) || is_observed_global(interferer, i);
		if (is_global) {
			for (std::size_t j = 0; j < end; j++) {
				shape->set(i, j, intersection(dst.shape->at(i, j), interferer.shape->at(i, j)));
			}
		} else {
			for (std::size_t j = 0; j < end; j++) {
				shape->set(i, j, setunion(dst.shape->at(i, j), interferer.shape->at(i, j)));
			}
		}
	}

	// 1.1 extend shape: add locals of interfering thread
	assert(shape->size() == dst.shape->size() + shape->sizeLocals());
	for (std::size_t i = 0; i < interferer.shape->sizeLocals(); i++) {
		std::size_t src_col = interferer.shape->offset_locals(interferer_tid) + i;
		std::size_t dst_col = dst.shape->size() + i;
		
		// add extended.locals ~ extended.locals
		for (std::size_t j = i; j < interferer.shape->sizeLocals(); j++) {
			std::size_t src_row = interferer.shape->offset_locals(interferer_tid) + j;
			std::size_t dst_row = dst.shape->size() + j;
			shape->set(dst_row, dst_col, interferer.shape->at(src_row, src_col));
		}

		// add specials/non-locals/MM-tid0-locals ~ extended.locals
		for (std::size_t j = 0; j < shape->offset_locals(interferer_tid); j++) {
			std::size_t src_row = j;
			std::size_t dst_row = j;
			shape->set(dst_row, dst_col, interferer.shape->at(src_row, src_col));
		}

		// add interferer-tid.locals ~ extended-tid.locals
		for (std::size_t j = dst.shape->offset_locals(interferer_tid); j < dst.shape->size(); j++) {
			std::size_t src_row = j;
			std::size_t dst_row = j;
			shape->set(dst_row, dst_col, PRED);

			// restrict the possible relations between local variables of different threads
			// according to ownership
			bool is_row_owned = false;
			bool is_col_owned = false;

			switch (msetup) {
				case GC:
					is_row_owned = dst.own.is_owned(src_row);
					is_col_owned = interferer.own.is_owned(src_col);
					// TODO: fall-through to MM?
					// break;
				case MM:
					is_row_owned |= !is_reachable(*dst.shape, src_row);
					is_col_owned |= !is_reachable(*interferer.shape, src_col);
					break;
				case PRF:
					is_row_owned = dst.own.is_owned(src_row);
					is_col_owned = interferer.own.is_owned(src_col);
					is_row_owned &= !is_reachable(*dst.shape, src_row);
					is_col_owned &= !is_reachable(*interferer.shape, src_col);
					#if INTERFERENCE_OPTIMIZATION
						// TODO: still needed?
						if (interferer.pc[0]->clazz() != Statement::ATOMIC) {
							// is_row_owned = false;
							// is_col_owned = false;
							if (dst.shape->test(src_row, dst.shape->index_FREE(), MT)) {
								is_row_owned = false;
								is_col_owned = false;
							}
						}
					#endif
			}
			
			if (is_row_owned && is_col_owned)
				shape->set(dst_row, dst_col, BT_);
			else if (is_row_owned)
				shape->set(dst_row, dst_col, MT_GT_BT);
			else if (is_col_owned)
				shape->set(dst_row, dst_col, MF_GF_BT);
		}
	}

	if (msetup == PRF) {
		for (std::size_t i = 0; i < shape->sizeLocals(); i++)
			for (std::size_t j = 0; j < shape->sizeLocals(); j++) {
				std::size_t row = shape->offset_locals(interferer_tid) + i;
				std::size_t src_col = shape->offset_locals(interferer_tid) + j;
				std::size_t col = shape->offset_locals(extended_tid) + j;
				if (shape->test(row, shape->index_FREE(), MT))
					if (interferer.own.is_owned(src_col) && interferer.shape->at(src_col, shape->index_UNDEF()) != MT_) {
						for (std::size_t t = 0; t < shape->size(); t++)
							shape->set(row, t, setunion(shape->at(row, t), shape->at(col, t)));
						shape->set(row, row, EQ_);
						shape->set(row, shape->index_UNDEF(), BT_);
					}
			}
	}

	// 1.2 extend shape: remove inconsistent predicates
	bool needs_iterating;
	do {
		needs_iterating = false;
		for (std::size_t i = 0; i < shape->size(); i++)
			for (std::size_t j = i+1; j < shape->size(); j++) {
				for (Rel r : shape->at(i, j))
					if (!consistent(*shape, i, j, r)) {
						shape->remove_relation(i, j, r);
						needs_iterating = true;
					}
				if (shape->at(i, j).none()) {
					delete shape;
					return NULL;
				}
			}
	} while (needs_iterating);
	assert(consistent(*shape));

	// 2. extend ages
	Cfg* result = new Cfg(dst, shape);
	Cfg& res = *result;

	res.ages->extend(res.shape->sizeLocals());
	for (std::size_t i = 0; i < interferer.shape->sizeLocals(); i++) {
		std::size_t src_col = interferer.shape->offset_locals(interferer_tid) + i;
		std::size_t dst_col = dst.shape->size() + i;
		
		// add extended.locals ~ extended.locals
		for (std::size_t j = i; j < interferer.shape->sizeLocals(); j++) {
			std::size_t src_row = interferer.shape->offset_locals(interferer_tid) + j;
			std::size_t dst_row = dst.shape->size() + j;
			
			for (bool br : {false, true})
				for (bool bc : {false, true})
					res.ages->set(dst_row, br, dst_col, bc, interferer.ages->at(src_row, br, src_col, bc));
		}

		// add specials/non-locals/MM-tid0-locals ~ extended.locals
		for (std::size_t j = 0; j < res.shape->offset_locals(interferer_tid); j++) {
			std::size_t src_row = j;
			std::size_t dst_row = j;

			for (bool br : {false, true})
				for (bool bc : {false, true})
					res.ages->set(dst_row, br, dst_col, bc, interferer.ages->at(src_row, br, src_col, bc));
		}
	}
	for (std::size_t i = 0; i < interferer.shape->sizeLocals(); i++) {
		std::size_t dst_col = dst.shape->size() + i;

		// add interferer-tid.locals ~ extended-tid.locals
		for (std::size_t j = dst.shape->offset_locals(interferer_tid); j < dst.shape->size(); j++) { // TODO: j = ... is wrong
			std::size_t dst_row = j;

			for (bool br : {false, true})
				for (bool bc : {false, true}) {
					// derive transitive relations via global/MM-tid1-local variables
					AgeRel transitive = AgeRel::BOT;
					for (std::size_t t = 0; t < res.shape->offset_locals(interferer_tid); t++) 
						for (bool bt : {false, true}) {
							auto trans = mk_trans_rel(*res.ages, dst_row, br, t, bt, dst_col, bc);
							if (trans != AgeRel::BOT) {
								transitive = trans;
								break;
							}
							// if (trans == AgeRel::BOT) continue;
							// if (transitive == AgeRel::BOT)
							//	transitive = trans;
							// else if (trans != transitive) {
							//	std::cout << "BAD TRANSITIVITY" << std::endl;
							//	std::cout << "row-t-col: " << dst_row << "-" << t << "-" << dst_col << std::endl;
							//	std::cout << transitive << " vs. " << trans << std::endl << std::endl;
							//	std::cout << "do ages match? -> " << do_ages_match(dst, interferer, msetup) << std::endl;
							//	std::cout << "c1: " << dst << *dst.shape << *dst.ages;
							//	std::cout << "c2: " << interferer << *interferer.shape << *interferer.ages;
							//	std::cout << "ex: " << res << *res.shape << *res.ages;
							//	throw std::runtime_error("Inconsistent AgeMatrix leading to bad transitivity during Interference");
							// }
						}
					res.ages->set(dst_row, br, dst_col, bc, transitive);
				}
		}
	}

	// 3. extend inout
	res.inout[extended_tid] = interferer.inout[interferer_tid];

	// 4. extend pc
	assert(interferer.pc[interferer_tid] != NULL);
	res.pc[extended_tid] = interferer.pc[interferer_tid];

	// 5. nothing to do for seen
	for (std::size_t i = 0; i < res.seen.size(); i++)
		res.seen[i] = dst.seen[i] | interferer.seen[i];

	// 6. nothing to do for state

	// 7. ownership + sin
	for (std::size_t i = 0; i < dst.shape->sizeLocals(); i++) {
		res.own.set_ownership(dst.shape->size() + i, interferer.own.is_owned(dst.shape->offset_locals(0) + i));
		res.sin[dst.shape->size() + i] = interferer.sin[dst.shape->offset_locals(0) + i];
	}

	// 8. extend oracle
	res.oracle[extended_tid] = interferer.oracle[interferer_tid];

	return result;
}


/******************************** PROJECTION ********************************/

void project_away(Cfg& cfg, unsigned short extended_thread_tid) {
	assert(extended_thread_tid == 1 || extended_thread_tid == 2);

	// reset ownership to dummy value
	for (std::size_t i = cfg.shape->offset_locals(extended_thread_tid); i < cfg.shape->size(); i++) {
		cfg.own.publish(i);
		cfg.sin[i] = false;
	}

	// remove extended thread from cfg
	cfg.shape->shrink();
	cfg.ages->shrink(cfg.shape->sizeLocals());

	cfg.pc[extended_thread_tid] = NULL;
	cfg.inout[extended_thread_tid] = OValue();
	cfg.oracle[extended_thread_tid] = false;

	// the state does not need to be changed

	// seen does not need to be changed ~> if the extended thread add a value to seen, then
	// only he can add this value to the datastructure (by data independence argument) and
	// thus we do not want to remove anything from there
}


/******************************** INTERFERENCE FOR TID ********************************/

std::vector<Cfg> mk_one_interference(const Cfg& c1, const Cfg& c2, MemorySetup msetup) {
	// this function assumes that c2 can interfere c1
	// extend c1 with (parts of) c2, compute post on the extended cfg, project away the extended part
	unsigned short extended_thread_tid = msetup == MM ? 2 : 1; // 0-indexed

	// extend c1 with c2 (we need a temporary cfg since we cannot/shouldnot modify cfgs that stored in the encoding)
	std::unique_ptr<Cfg> extended((extend_cfg(c1, c2, extended_thread_tid, msetup)));
	if (!extended) return {};

	const Cfg& tmp = *extended;
	assert(tmp.shape != NULL);

	// do one post step for the extended thread
	assert(tmp.pc[extended_thread_tid] != NULL);
	INTERFERENCE_STEPS++;
	std::vector<Cfg> postcfgs = tmr::post(tmp, extended_thread_tid, msetup);
	
	// the resulting cfgs need to be projected to 1/2 threads, then push them to result vector
	for (Cfg& pcfg : postcfgs)
		project_away(pcfg, extended_thread_tid);

	return std::move(postcfgs);
}


/******************************** INTERFERENCE FOR ALL THREADS ********************************/

void mk_regional_interference(RemainingWork& work, Encoding::__sub__store__& region, MemorySetup msetup, std::size_t& counter) {
	// begin = first cfg
	// end = post last cfg

	std::size_t old_size;
	do {
		old_size = region.size();
		for (auto it1 = region.begin(); it1 != region.end(); it1++) {
			const Cfg& c1 = *it1;
			if (c1.pc[0] == NULL) continue;
			if (msetup == MM && c1.pc[1] == NULL) continue;

			for (auto it2 = it1; it2 != region.end(); it2++) {
				const Cfg& c2 = *it2;
				if (c2.pc[0] == NULL) continue;
				if (msetup == MM && c2.pc[1] == NULL) continue;			

				if (can_interfere(c1, c2, msetup)) {
					work.add(mk_one_interference(c1, c2, msetup));
					counter++;
				}
				if (can_interfere(c2, c1, msetup)) {
					work.add(mk_one_interference(c2, c1, msetup));
					counter++;
				}
			}
		}
	} while (old_size < region.size());
}

void tmr::mk_all_interference(Encoding& enc, RemainingWork& work, MemorySetup msetup) {
	std::cerr << "interference...   ";
	std::size_t counter = 0;

	for (auto& kvp : enc) {
		std::cerr << "[" << kvp.second.size() << "-" << enc.size()/1000 << "k]";
		mk_regional_interference(work, kvp.second, msetup, counter);
	}

	std::cerr << " done! [enc.size()=" << enc.size() << ", matches=" << counter << "]" << std::endl;
}
