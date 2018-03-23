#include "fixp/interference.hpp"

#include <stdexcept>
#include "helpers.hpp"
#include "counter.hpp"
#include "config.hpp"

using namespace tmr;


// /******************************** CHECK MATCH ********************************/

bool is_observed_owned(const Cfg& cfg, std::size_t obs) {
	std::size_t begin = cfg.shape->offset_locals(0);
	std::size_t end = begin + cfg.shape->sizeLocals();
	for (std::size_t i = begin; i < end; i++)
		if (cfg.own.at(i) && cfg.shape->test(obs, i, EQ))
			return true;
	return false;
}

bool is_observed_global(const Cfg& cfg, std::size_t obs) {
	for (std::size_t j = 5; j < cfg.shape->offset_locals(0); j++)
		if (intersection(cfg.shape->at(obs, j), MT_GT_BT).none())
			return true;
	return false;
}

bool do_shapes_match(const Cfg& cfg, const Cfg& interferer) {
	std::size_t end = cfg.shape->offset_locals(0);

	assert(cfg.shape->sizeObservers() == 2);
	assert(cfg.shape->index_ObserverVar(0) == 3);
	assert(cfg.shape->index_ObserverVar(1) == 4);

	for (std::size_t i = 0; i < end; i++) {
		// if (i == 3 || i == 4) continue;
		for (std::size_t j = i+1; j < end; j++) {
			// if (j == 3 || j == 4) continue;
			if (intersection(cfg.shape->at(i, j), interferer.shape->at(i, j)).none())
				return false;
		}
	}

	// for (std::size_t i : {3, 4}) {
	// 	if (cfg.shape->at(i, cfg.shape->index_UNDEF()) == MT_ ^ interferer.shape->at(i, interferer.shape->index_UNDEF()) == MT_) {
	// 		return false;
	// 	}

	// 	bool is_global = is_observed_global(cfg, i) || is_observed_global(interferer, i);
	// 	if (!is_global) continue;

	// 	bool is_owned = is_observed_owned(cfg, i) || is_observed_owned(interferer, i);
	// 	if (is_owned) continue;

	// 	for (std::size_t j = 0; j < end; j++)
	// 		if (intersection(cfg.shape->at(i, j), interferer.shape->at(i, j)).none())
	// 			return false;
	// }

	return true;
}

static bool is_noop(const Statement& pc) {
	switch (pc.clazz()) {
		case Statement::SQZ:       return true;
		case Statement::WHILE:     return true;
		// case Statement::OUTPUT: return true;
		case Statement::BREAK:     return true;
		case Statement::ORACLE:    return true;
		case Statement::CHECKP:    return true;
		case Statement::HPSET:     return true;
		case Statement::HPRELEASE: return true;
		// #if KILL_IS_NOOP
		// 	case Statement::KILL:  return true;
		// #endif
		default: return false;
	}
}

// static bool can_skip(const Cfg& cfg) {
// 	#define SKIP {INTERFERENCE_SKIPPED++; return true;}
// 	#define NO_SKIP {return false;}
	
// 	const Statement& pc = *cfg.pc[0];
// 	std::size_t tmp;

// 	auto set_tmp = [&] (const Expr& expr) {
// 		tmp = mk_var_index(*cfg.shape, expr, 0);
// 	};

// 	switch (pc.clazz()) {
// 		case Statement::ITE:
// 			if (static_cast<const Ite&>(pc).cond().type() != Condition::CASC) SKIP;
// 			NO_SKIP;
// 		case Statement::MALLOC:
// 			set_tmp(static_cast<const Malloc&>(pc).var());
// 			// malloc means redirecting pointers
// 			if (!cfg.freed && tmp >= cfg.shape->offset_locals(0)) SKIP; // we are not interested in local redirections
// 			NO_SKIP; // ...but want to see redirecting globals...
// 		case Statement::CAS:
// 			set_tmp(static_cast<const CompareAndSwap&>(pc).dst());
// 			break;
// 		case Statement::ASSIGN:
// 			set_tmp(static_cast<const Assignment&>(pc).lhs());
// 			break;
// 		case Statement::INPUT:
// 			if (cfg.inout[0].type() != OValue::OBSERVABLE) {
// 				set_tmp(static_cast<const ReadInputAssignment&>(pc).expr());
// 				break;
// 			}
// 			NO_SKIP;
// 		case Statement::SETNULL:
// 			set_tmp(static_cast<const NullAssignment&>(pc).lhs());
// 			break;
// 		default:
// 			NO_SKIP;
// 	}

// 	if (cfg.own.at(tmp)) SKIP;
// 	NO_SKIP;
// }

// static bool can_skip_victim(const Cfg& cfg) {
// 	#define SKIP {INTERFERENCE_SKIPPED++; return true;}
// 	#define NO_SKIP {return false;}
	
// 	const Statement& pc = *cfg.pc[0];
// 	std::size_t tmp;

// 	auto set_tmp = [&] (const Expr& expr) {
// 		tmp = mk_var_index(*cfg.shape, expr, 0);
// 	};

// 	switch (pc.clazz()) {
// 		case Statement::ITE:
// 			if (static_cast<const Ite&>(pc).cond().type() != Condition::CASC) SKIP;
// 			// TODO: if (victim && cont lhs and rhs are local) SKIP ??????;
// 			NO_SKIP;
// 		// case Statement::ASSIGN:
// 		//	if (!cfg.own.is_owned(mk_var_index(*cfg.shape, static_cast<const Assignment&>(pc).rhs(), 0))) NO_SKIP;
// 		//	set_tmp(static_cast<const Assignment&>(pc).lhs());
// 		//	break;
// 		case Statement::INPUT:
// 			set_tmp(static_cast<const ReadInputAssignment&>(pc).expr());
// 			break;
// 		case Statement::SETNULL:
// 			set_tmp(static_cast<const NullAssignment&>(pc).lhs());
// 			break;
// 		// case Statement::MALLOC:
// 		//	set_tmp(static_cast<const Malloc&>(pc).var());
// 		//	if (tmp >= cfg.shape->offset_locals(0)) SKIP;
// 		//	NO_SKIP;
// 		// case Statement::FREE:
// 		//	set_tmp(static_cast<const Free&>(pc).var());
// 		//	break;
// 		default:
// 			NO_SKIP;
// 	}

// 	if (cfg.own.at(tmp)) SKIP;
// 	NO_SKIP;
// }

bool can_interfere(const Cfg& cfg, const Cfg& interferer) {
	// check whether 'interferer' can interfere with cfg
	unsigned short interferer_tid = 0;

	// 0. Optimizations
	#if SKIP_NOOPS
		if (is_noop(*cfg.pc[0])) return false;
		if (is_noop(*interferer.pc[0])) return false;
	#endif

	// #if INTERFERENCE_OPTIMIZATION
	// 	if (can_skip_victim(cfg)) return false;
	// 	if (can_skip(interferer)) return false;
	// #endif

	// 1. REUSE must be in the same state
	if (cfg.freed != interferer.freed || cfg.retired != interferer.retired)
		return false;

	// 2. cfg.state must match interferer.state
	if (!(cfg.state == interferer.state))
		return false;

	// 3. prevent some data combinations which are excluded by the data independence argument
	if (cfg.inout[0] == interferer.inout[0] && cfg.inout[0].type() != OValue::DUMMY)
		return false;

	// 4. interfering thread should not be behind or ahead (too far) of seen
	//    A thread can be ahead of the values which have been added and removed from the data structure
	//    if they are about to add one.
	//    So we just ensure that two "out"-threads have seen the same values.
	//    For everything else, we rely on the later shape comparison (if a value was seen
	//    it is not in relation with UNDEF, otherwise it is).
	if (cfg.seen != interferer.seen && !cfg.pc[0]->function().has_input() && !interferer.pc[interferer_tid]->function().has_input())
		return false;
	// for (std::size_t i : {0, 1}) {
	// 	if (cfg.seen[i] != interferer.seen[i]) {
	// 		bool cfgin = cfg.pc[0]->function().has_input() && cfg.inout[0].type() == OValue::OBSERVABLE && cfg.inout[0].id() == i;
	// 		bool intin = interferer.pc[0]->function().has_input() && interferer.inout[0].type() == OValue::OBSERVABLE && interferer.inout[0].id() == i;
	// 		if (!cfgin && !intin) return false;
	// 	}
	// }

	// 5. cfg.shape must be a superset of interferer.shape on the non-local cell terms
	if (!do_shapes_match(cfg, interferer))
		return false;

	return true;
}


// /******************************** EXTENSION ********************************/

bool is_reachable(const Shape& shape, std::size_t cid) {
	// this is only used for MM
	for (std::size_t i = shape.offset_vars(); i < shape.offset_locals(1); i++)
		if (haveCommon(shape.at(i, cid), EQ_MT_GT))
			return false;
	return true;
}

Cfg* extend_cfg(const Cfg& dst, const Cfg& interferer, unsigned short extended_tid) {
	// extend dst with interferer (create a copy of dst, then extend the copy)
	unsigned short interferer_tid = 0;
	#define ABORT { delete shape; return NULL; }

	// 1.0 extend shape
	Shape* shape = new Shape(*dst.shape);	
	shape->extend();

	// some cells can be intersected in advance, have to be unified
	std::size_t end = dst.shape->offset_locals(0);
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
			// TODO: copy if owned, do setunion only if necessary?
			for (std::size_t j = 0; j < end; j++) {
				shape->set(i, j, setunion(dst.shape->at(i, j), interferer.shape->at(i, j)));
			}
		}
	}

	// 1.1 extend shape: add locals of interfering thread
	for (std::size_t i = 0; i < interferer.shape->sizeLocals(); i++) {
		std::size_t src_col = interferer.shape->offset_locals(0) + i;
		std::size_t dst_col = dst.shape->size() + i;
		
		// add extended.locals ~ extended.locals
		for (std::size_t j = i; j < interferer.shape->sizeLocals(); j++) {
			std::size_t src_row = interferer.shape->offset_locals(0) + j;
			std::size_t dst_row = dst.shape->size() + j;
			shape->set(dst_row, dst_col, interferer.shape->at(src_row, src_col));
		}

		// add specials/non-locals ~ extended.locals
		for (std::size_t j = 0; j < shape->offset_locals(0); j++) {
			std::size_t src_row = j;
			std::size_t dst_row = j;
			shape->set(dst_row, dst_col, interferer.shape->at(src_row, src_col));
		}

		// add interferer-tid.locals ~ extended-tid.locals
		for (std::size_t j = dst.shape->offset_locals(0); j < dst.shape->size(); j++) {
			std::size_t src_row = j;
			std::size_t dst_row = j;
			shape->set(dst_row, dst_col, PRED);

			// restrict the possible relations between local variables of different threads
			bool is_row_owned = dst.own.at(src_row);
			bool is_col_owned = interferer.own.at(src_col);

			bool is_row_valid = dst.valid_ptr.at(src_row);
			bool is_col_valid = interferer.valid_ptr.at(src_col);
			
			if (is_row_owned && is_col_owned)
				shape->set(dst_row, dst_col, BT_);
			else if ((is_row_owned && is_col_valid) || (is_col_owned && is_row_valid))
				ABORT //shape->set(dst_row, dst_col, RelSet(0));
			else if (is_row_owned)
				shape->set(dst_row, dst_col, MT_GT_BT);
			else if (is_col_owned)
				shape->set(dst_row, dst_col, MF_GF_BT);
		}
	}

	// TODO: what??
	// for (std::size_t i = 0; i < shape->sizeLocals(); i++)
	// 	for (std::size_t j = 0; j < shape->sizeLocals(); j++) {
	// 		std::size_t row = shape->offset_locals(interferer_tid) + i;
	// 		std::size_t src_col = shape->offset_locals(interferer_tid) + j;
	// 		std::size_t col = shape->offset_locals(extended_tid) + j;
	// 		if (shape->test(row, shape->index_FREE(), MT))
	// 			if (interferer.own.is_owned(src_col) && interferer.shape->at(src_col, shape->index_UNDEF()) != MT_) {
	// 				for (std::size_t t = 0; t < shape->size(); t++)
	// 					shape->set(row, t, setunion(shape->at(row, t), shape->at(col, t)));
	// 				shape->set(row, row, EQ_);
	// 				shape->set(row, shape->index_UNDEF(), BT_);
	// 			}
	// 	}
	// }

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

	// 2. create new cfg
	Cfg* result = new Cfg(dst, shape);
	Cfg& res = *result;

	// 3. extend inout
	res.inout[extended_tid] = interferer.inout[interferer_tid];

	// 4. extend pc
	res.pc[extended_tid] = interferer.pc[interferer_tid];

	// 5. extend seen
	for (std::size_t i = 0; i < res.seen.size(); i++)
		res.seen[i] = dst.seen[i] | interferer.seen[i];

	// 6. nothing to do for state

	// 7. ownership, validity, smr states
	for (std::size_t i = 0; i < dst.shape->sizeLocals(); i++) {
		auto di = dst.shape->size() + i;
		auto si = dst.shape->offset_locals(0) + i;
		res.own.set(di, interferer.own.at(si));
		res.valid_ptr.set(di, interferer.valid_ptr.at(si));
		res.valid_next.set(di, interferer.valid_next.at(si));
		res.guard0state.set(di, interferer.guard0state.at(si));
		res.guard1state.set(di, interferer.guard1state.at(si));
	}

	// 8. extend oracle
	res.oracle[extended_tid] = interferer.oracle[interferer_tid];

	return result;
}


// /******************************** PROJECTION ********************************/

static inline void project_away(Cfg& cfg, unsigned short extended_thread_tid) {
	// reset own, valid, smr states to default value
	for (std::size_t i = cfg.shape->offset_locals(extended_thread_tid); i < cfg.shape->size(); i++) {
		cfg.own.set(i, true);
		cfg.valid_ptr.set(i, false);
		cfg.valid_next.set(i, false);
		cfg.guard0state.set(i, nullptr);
		cfg.guard1state.set(i, nullptr);
	}

	// remove extended thread from cfg
	cfg.shape->shrink();

	cfg.pc[extended_thread_tid] = NULL;
	cfg.inout[extended_thread_tid] = OValue();
	cfg.oracle[extended_thread_tid] = false;

	// seen does not need to be changed ~> if the extended thread add a value to seen, then
	// only he can add this value to the datastructure (by data independence argument) and
	// thus we do not want to remove anything from there
}


// /******************************** INTERFERENCE ********************************/

std::vector<Cfg> mk_one_interference(const Cfg& c1, const Cfg& c2) {
	// std::cout << "===============================" << std::endl;
	// std::cout << "Interference for: " << c1 << *c1.shape << std::endl;
	// std::cout << "and: " << c2 << *c2.shape << std::endl;
	// std::cout << "-------------------------------" << std::endl;

	// this function assumes that c2 can interfere c1
	// extend c1 with (parts of) c2, compute post on the extended cfg, project away the extended part
	unsigned short extended_thread_tid = 1; // 0-indexed

	// extend c1 with c2 (we need a temporary cfg since we cannot/shouldnot modify cfgs that stored in the encoding)
	std::unique_ptr<Cfg> extended((extend_cfg(c1, c2, extended_thread_tid)));
	if (!extended) return {};

	const Cfg& tmp = *extended;
	// std::cout << "Extension is: " << tmp << *tmp.shape << std::endl;
	// if (SEQUENTIAL_STEPS > 90000 && tmp.pc[0]->id()==22) {std::cout << "Interference post for: " << tmp << *tmp.shape << std::endl;}

	// do one post step for the extended thread
	INTERFERENCE_STEPS++;
	std::vector<Cfg> postcfgs = tmr::post(tmp, extended_thread_tid);

	
	// the resulting cfgs need to be projected to 1 threads, then push them to result vector
	// std::cout << std::endl << std::endl << "interference: " << tmp << *tmp.shape;
	for (Cfg& pcfg : postcfgs) {
		// std::cout << "Post: " << pcfg << *pcfg.shape << std::endl;
		project_away(pcfg, extended_thread_tid);
	}

	return postcfgs;
}


/******************************** INTERFERENCE FOR ALL THREADS ********************************/

void mk_regional_interference(RemainingWork& work, Encoding::__sub__store__& region, std::size_t& counter) {
	// begin = first cfg
	// end = post last cfg

	// std::size_t old_size;
	// do {
	// 	old_size = region.size();
		for (auto it1 = region.begin(); it1 != region.end(); it1++) {
			const Cfg& c1 = *it1;
			if (c1.pc[0] == NULL) continue;

			for (auto it2 = it1; it2 != region.end(); it2++) {
				const Cfg& c2 = *it2;
				if (c2.pc[0] == NULL) continue;

				if (can_interfere(c1, c2)) {
					work.add(mk_one_interference(c1, c2));
					work.add(mk_one_interference(c2, c1));
					counter++;
				}
			}
		}
	// } while (old_size < region.size());
}

void tmr::mk_all_interference(Encoding& enc, RemainingWork& work) {	
		std::cerr << "interference...   ";
		std::size_t counter = 0;

		// std::cout << std::endl << std::endl << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;
		// std::cout << "*******************************************************************************************" << std::endl;

		std::size_t bucket_counter = 0;
		for (auto& kvp : enc) {
			// std::cerr << "[" << kvp.second.size() << "-" << enc.size()/1000 << "k]";
			mk_regional_interference(work, kvp.second, counter);

			bucket_counter++;
			if (bucket_counter%100 == 0) std::cerr << "[" << bucket_counter << "/" << enc.bucket_count() << "]";
		}

		std::cerr << " done! [enc.size()=" << enc.size() << ", matches=" << counter << ", enc.bucket_count()=" << enc.bucket_count() << "]" << std::endl;
}
