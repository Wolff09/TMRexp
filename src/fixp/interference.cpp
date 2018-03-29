#include "fixp/interference.hpp"

#include <stdexcept>
#include "helpers.hpp"
#include "counter.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** CHECK MATCH ********************************/

static inline bool is_noop(const Statement& pc) {
	switch (pc.clazz()) {
		case Statement::SQZ:       return true;
		case Statement::WHILE:     return true;
		case Statement::BREAK:     return true;
		case Statement::ORACLE:    return true;
		case Statement::CHECKP:    return true;
		case Statement::HPRELEASE: return true;
		case Statement::OUTPUT:    return true;
		#if KILL_IS_NOOP
			case Statement::KILL:  return true;
		#endif
		default: return false;
	}
}

static inline bool do_shapes_match(const Cfg& cfg, const Cfg& interferer) {
	std::size_t end = cfg.shape->offset_locals(0);
	for (std::size_t i = 0; i < end; i++) {
		for (std::size_t j = i+1; j < end; j++) {
			if (intersection(cfg.shape->at(i, j), interferer.shape->at(i, j)).none())
				return false;
		}
	}
	return true;
}

static inline bool can_interfere(const Cfg& cfg, const Cfg& interferer) {
	// 0. skipping statements that have no effect
	if (is_noop(*cfg.pc[0])) return false;
	if (is_noop(*interferer.pc[0])) return false;

	// 1. REUSE address must be in the same state
	if (cfg.freed != interferer.freed || cfg.retired != interferer.retired)
		return false;

	// 2. cfg.state must match interferer.state
	if (!(cfg.state == interferer.state))
		return false;

	// 3. prevent data combinations excluded by data independence (no two threards can have the same observable input/output)
	if (cfg.inout[0] == interferer.inout[0] && cfg.inout[0].type() == OValue::OBSERVABLE)
		return false;

	// 4. seen must be the same (cf. to initial configurations)
	if (cfg.seen != interferer.seen)
		return false;

	// 5. cfg.shape must be a superset of interferer.shape on the non-local cell terms
	if (!do_shapes_match(cfg, interferer))
		return false;

	return true;
}


/******************************** EXTENSION ********************************/

Cfg* extend_cfg(const Cfg& dst, const Cfg& interferer) {
	// extend dst with interferer (create a copy of dst, then extend the copy)

	// 1.0 extend shape
	Shape* shape = new Shape(*dst.shape);	
	shape->extend();

	// 1.1 extend shape: correlate shared information (intersection between victim and interferer shape)
	std::size_t end = dst.shape->offset_locals(0);
	for (std::size_t i = 0; i < end; i++) {
		for (std::size_t j = i+1; j < end; j++) {
			shape->set(i, j, intersection(dst.shape->at(i, j), interferer.shape->at(i, j)));
		}
	}

	// 1.2 extend shape: add locals of interfering thread
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
			// shape->set(dst_row, dst_col, PRED);

			// restrict the possible relations between local variables of different threads
			bool is_row_owned = dst.own.at(src_row);
			bool is_col_owned = interferer.own.at(src_col);

			bool is_row_valid = dst.valid_ptr.at(src_row);
			bool is_col_valid = interferer.valid_ptr.at(src_col);

			bool is_row_retired = (dst.guard0state.at(src_row) && dst.guard0state.at(src_row)->is_special())
			                   || (dst.guard1state.at(src_row) && dst.guard1state.at(src_row)->is_special());
			bool is_col_retired = (dst.guard0state.at(src_col) && dst.guard0state.at(src_col)->is_special())
			                   || (dst.guard1state.at(src_col) && dst.guard1state.at(src_col)->is_special());

			bool is_row_reuse = dst.shape->test(src_row, dst.shape->index_REUSE(), EQ);
			bool is_col_reuse = interferer.shape->test(src_col, interferer.shape->index_REUSE(), EQ);

			// if (is_row_owned && !is_col_valid) {
			// 	if (is_row_reuse && dst.shape->at(src_row, dst.shape->index_REUSE()) != EQ_) {
			// 		std::cout << dst.shape->at(src_row, dst.shape->index_REUSE()) << "  \t" << interferer.shape->at(src_row, interferer.shape->index_REUSE()) << std::endl;

			// 		cond:

			// 		((is_row_owned && !is_col_valid) || (is_col_owned && !is_row_valid)) && (!is_row_reuse || !is_col_reuse)   ==>   shape->set(dst_row, dst_col, MT_GT_MF_GF_BT); // owned cannot be reached

			// 	}
			// }

			// if (is_row_owned && is_col_owned) {
			// 	shape->set(dst_row, dst_col, BT_);
			// } else if ((is_row_owned && is_col_valid) || (is_col_owned && is_row_valid)) {
			// 	// ownership guarantees that other threads have only invalid pointers
			// 	// delete shape;
			// 	// return NULL;
			// 	shape->set(dst_row, dst_col, BT_);
			// 	// TODO: this is wrong! results in BT_ cell
			// // } else if (is_row_owned && !is_col_valid) {
			// // 	if (is_row_reuse) {
			// //		// not correct
			// // 		shape->set(dst_row, dst_col, EQ_);
			// // 		shape->set(dst_row, shape->index_REUSE(), EQ_);
			// // 	} else {
			// // 		shape->set(dst_row, dst_col, BT_);
			// // 	}
			// // } else if (is_col_owned && !is_row_valid) {
			// // 	if (is_col_reuse) {
			// //		// not correct
			// // 		shape->set(dst_row, dst_col, EQ_);
			// // 		shape->set(dst_col, shape->index_REUSE(), EQ_);
			// // 	} else {
			// // 		shape->set(dst_row, dst_col, BT_);
			// // 	}
			// } else if (is_row_retired ^ is_col_retired) {
			// 	// a cell cannot be both retired and not retired
			// 	shape->set(dst_row, dst_col, MT_GT_MF_GF_BT);
			// } else if ((is_row_valid ^ is_col_valid) && (!is_row_reuse || !is_col_reuse)) {
			// 	// pointers to a cell can only be invalidated by a free
			// 	// if a non-REUSE cell is free, then all pointers to it are invalid
			// 	shape->set(dst_row, dst_col, MT_GT_MF_GF_BT);
			// } else {
			// 	shape->set(dst_row, dst_col, PRED);
			// }

			RelSet cell;

			if (is_row_owned && is_col_valid) {
				cell |= EQ_MF_GF;
			}
			if (is_row_valid && is_col_owned) {
				cell |= EQ_MT_GT;
			}
			if (is_row_owned && is_col_owned) {
				cell |= EQ_MT_MF_GT_GF; // consequence of previous?
			}
			if (is_row_retired ^ is_col_retired) {
				cell.set(EQ);
			}
			if ((is_row_valid ^ is_col_valid) && (!is_row_reuse || !is_col_reuse)) {
				cell.set(EQ);
			}
			if ((is_row_owned ^ !is_col_valid) && (!is_row_reuse || !is_col_reuse)) {
				cell.set(EQ); // consequence of previous?
			}
			if ((!is_row_valid ^ is_col_owned) && (!is_row_reuse || !is_col_reuse)) {
				cell.set(EQ); // consequence of previous?
			}

			if (!is_row_valid && (!is_row_reuse || dst.freed)) {
				cell |= MT_GT;
			}
			if (!is_col_valid && (!is_col_reuse || dst.freed)) {
				cell |= MF_GF;
			}

			shape->set(dst_row, dst_col, cell.flip());
		}
	}

	// 1.3 extend shape: remove inconsistent predicates
	bool success = make_concretisation(*shape);
	if (!success) {
		delete shape;
		return NULL;
	}

	// 1.4 extend shape: re-try removing relations as shape might be more precise now
	for (std::size_t row = shape->offset_locals(0); row < shape->offset_locals(1); row++) {
		for (std::size_t col = shape->offset_locals(1); col < shape->size(); col++) {
			// bool is_row_valid = dst.valid_ptr.at(row);
			// bool is_col_valid = interferer.valid_ptr.at(col - shape->sizeLocals());

			// bool is_row_reuse = shape->test(row, shape->index_REUSE(), EQ);
			// bool is_col_reuse = shape->test(col, shape->index_REUSE(), EQ);

			// if ((is_row_valid ^ is_col_valid) && (!is_row_reuse || !is_col_reuse)) {
			// 	// pointers to a cell can only be invalidated by a free
			// 	// if a non-REUSE cell is free, then all pointers to it are invalid
			// 	shape->remove_relation(row, col, EQ);
			// }

			std::size_t src_row = row;
			std::size_t src_col = col - interferer.shape->offset_locals(0);

			bool is_row_owned = dst.own.at(src_row);
			bool is_col_owned = interferer.own.at(src_col);

			bool is_row_valid = dst.valid_ptr.at(src_row);
			bool is_col_valid = interferer.valid_ptr.at(src_col);

			bool is_row_retired = (dst.guard0state.at(src_row) && dst.guard0state.at(src_row)->is_special())
			                   || (dst.guard1state.at(src_row) && dst.guard1state.at(src_row)->is_special());
			bool is_col_retired = (dst.guard0state.at(src_col) && dst.guard0state.at(src_col)->is_special())
			                   || (dst.guard1state.at(src_col) && dst.guard1state.at(src_col)->is_special());

			bool is_row_reuse = shape->test(row, shape->index_REUSE(), EQ);
			bool is_col_reuse = shape->test(col, shape->index_REUSE(), EQ);

			RelSet cell;

			if (is_row_owned && is_col_valid) {
				cell |= EQ_MF_GF;
			}
			if (is_row_valid && is_col_owned) {
				cell |= EQ_MT_GT;
			}
			if (is_row_owned && is_col_owned) {
				cell |= EQ_MT_MF_GT_GF; // consequence of previous?
			}
			if (is_row_retired ^ is_col_retired) {
				cell.set(EQ);
			}
			if ((is_row_valid ^ is_col_valid) && (!is_row_reuse || !is_col_reuse)) {
				cell.set(EQ);
			}
			if ((is_row_owned ^ !is_col_valid) && (!is_row_reuse || !is_col_reuse)) {
				cell.set(EQ); // consequence of previous?
			}
			if ((!is_row_valid ^ is_col_owned) && (!is_row_reuse || !is_col_reuse)) {
				cell.set(EQ); // consequence of previous?
			}

			if (!is_row_valid && (!is_row_reuse || dst.freed)) {
				cell |= MT_GT;
			}
			if (!is_col_valid && (!is_col_reuse || dst.freed)) {
				cell |= MF_GF;
			}

			shape->set(row, col, shape->at(row, col) & cell.flip());

		}
	}
	success = make_concretisation(*shape);
	if (!success) {
		delete shape;
		return NULL;
	}

	// 2. create new cfg
	Cfg* result = new Cfg(dst, shape);
	Cfg& res = *result;

	// 3. extend inout
	res.inout[1] = interferer.inout[0];

	// 4. extend pc
	res.pc[1] = interferer.pc[0];

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
	res.oracle[1] = interferer.oracle[0];

	return result;
}


/******************************** PROJECTION ********************************/

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
	// seen does not need to be changed
}


/******************************** INTERFERENCE ********************************/

template<typename T>
static inline bool lhs_is_local(const T& stmt) {
	return stmt.lhs().clazz() == Expr::VAR && static_cast<const VarExpr&>(stmt.lhs()).decl().local();
}

static inline bool can_skip_interference(const Cfg& victim, const Cfg& interferer) {
	const auto& vpc = *victim.pc[0];
	const auto& ipc = *interferer.pc[0];

	// skip interferer if its action has a pure local effect
	switch (ipc.clazz()) {
		case Statement::ITE:
			if (static_cast<const Ite&>(ipc).cond().type() != Condition::CASC) return true; // only local updates
			break;
		case Statement::INPUT:
			if (interferer.inout[0].type() != OValue::OBSERVABLE) return true; // non-observable input
			break;
		case Statement::ASSIGN:
			if (lhs_is_local(static_cast<const Assignment&>(ipc))) return true; // assignment to local variable
			break;
		case Statement::SETNULL:
			if (lhs_is_local(static_cast<const NullAssignment&>(ipc))) return true; // assignment to local variable
			break;
		default:
			break;
	}

	// skip victim if its actions cannot be influenced
	switch (vpc.clazz()) {
		case Statement::INPUT:
			if (victim.inout[0].type() != OValue::OBSERVABLE) return true; // non-observable input
			break;
		case Statement::SETNULL:
			if (lhs_is_local(static_cast<const NullAssignment&>(vpc))) return true; // set local variable to null
			break;
		default:
			break;
	}

	return false;
}

std::vector<Cfg> mk_one_interference(const Cfg& c1, const Cfg& c2) {
	// more thorough, non-symmetric check than before
	if (can_skip_interference(c1, c2)) return {};

	// make combined cfg for c1 and c2
	std::unique_ptr<Cfg> extended((extend_cfg(c1, c2)));
	if (!extended) return {};

	const Cfg& tmp = *extended;
	INTERFERENCE_STEPS++;
	// std::cout << "===============================================" << std::endl << "Interference for: " << std::endl << c1 << *c1.shape << std::endl << "and:" << std::endl << c2 << *c2.shape << std::endl << "Post for combined:" << std::endl << tmp << *tmp.shape << std::endl;

	// do a post step for the extended thread
	std::vector<Cfg> postcfgs = tmr::post(tmp, 1);
	
	// the resulting cfgs need to be projected to 1 threads, then push them to result vector
	for (Cfg& pcfg : postcfgs) {
		// std::cout << "Post (before projection)" << std::endl << pcfg << *pcfg.shape << std::endl;

		project_away(pcfg, 1);
	}

	return postcfgs;
}


/******************************** INTERFERENCE FOR ALL THREADS ********************************/

void mk_regional_interference(RemainingWork& work, Encoding::__sub__store__& region, std::size_t& counter) {
	for (auto it1 = region.begin(); it1 != region.end(); it1++) {
		const Cfg& c1 = *it1;
		if (c1.pc[0] == NULL) continue;

		for (auto it2 = it1; it2 != region.end(); it2++) {
			const Cfg& c2 = *it2;
			if (c2.pc[0] == NULL) continue;

			if (can_interfere(c1, c2)) {
				work.add(mk_one_interference(c1, c2));
				work.add(mk_one_interference(c2, c1));
				counter += 2;
			}
		}
	}
}

void tmr::mk_all_interference(Encoding& enc, RemainingWork& work) {	
		std::cerr << "interference...   [" << enc.bucket_count() << " buckets" << "][#bucketsize]";
		std::size_t counter = 0;

		for (auto& kvp : enc) {
			std::cerr << "[" << kvp.second.size() << "]" << std::flush;

			mk_regional_interference(work, kvp.second, counter);
		}

		std::cerr << " done! [#enc=" << enc.size()/1000 << "." << (enc.size()-((enc.size()/1000)*1000))/100 << "k";
		std::cerr << ", #step=" << counter/1000 << "k";
		std::cerr << ", #steptotal=" << INTERFERENCE_STEPS/1000 << "k]" << std::endl;
}

void tmr::mk_cfg_interference(Encoding& enc, RemainingWork& work, const Cfg& cfg) {
	if (!cfg.pc[0]) return;
	auto& bucket = enc.get_bucket(cfg);
	for (auto it = bucket.begin(); it != bucket.end(); it++) {
		const Cfg& other = *it;
		if (other.pc[0] == NULL) continue;
		if (can_interfere(cfg, other)) {
			work.add(mk_one_interference(cfg, other));
			work.add(mk_one_interference(other, cfg));
		}
	}
}

