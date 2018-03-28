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
		// #if KILL_IS_NOOP
		// 	case Statement::KILL:  return true;
		// #endif
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
	// 0. Optimizations
	#if SKIP_NOOPS
		if (is_noop(*cfg.pc[0])) return false;
		if (is_noop(*interferer.pc[0])) return false;
	#endif

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
			shape->set(dst_row, dst_col, PRED);

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

			if (is_row_owned && is_col_owned) {
				shape->set(dst_row, dst_col, BT_);
			} else if ((is_row_owned && is_col_valid) || (is_col_owned && is_row_valid)) {
				// ownership guarantees that other threads have only invalid pointers
				delete shape;
				return NULL;
			} else if (is_row_retired ^ is_col_retired) {
				// a cell cannot be both retired and not retired
				shape->set(dst_row, dst_col, MT_GT_MF_GF_BT);
			} else if ((is_row_valid ^ is_col_valid) && (!is_row_reuse || !is_col_reuse)) {
				// pointers to a cell can only be invalidated by a free
				// if a non-REUSE cell is free, then all pointers to it are invalid
				shape->set(dst_row, dst_col, MT_GT_MF_GF_BT);
			} else {
				shape->set(dst_row, dst_col, PRED);
			}
		}
	}

	// 1.3 extend shape: remove inconsistent predicates
	bool success = make_concretisation(*shape);
	if (!success) {
		delete shape;
		return NULL;
	}

	// // 1.4 extend shape: re-try removing relations as shape might be more precise now
	// for (std::size_t row = shape->offset_locals(0); row < shape->offset_locals(1); row++) {
	// 	for (std::size_t col = shape->offset_locals(1); col < shape->size(); col++) {
	// 		bool is_row_valid = dst.valid_ptr.at(row);
	// 		bool is_col_valid = interferer.valid_ptr.at(col - shape->sizeLocals());

	// 		bool is_row_reuse = shape->test(row, shape->index_REUSE(), EQ);
	// 		bool is_col_reuse = shape->test(col, shape->index_REUSE(), EQ);

	// 		if ((is_row_valid ^ is_col_valid) && (!is_row_reuse || !is_col_reuse)) {
	// 			// pointers to a cell can only be invalidated by a free
	// 			// if a non-REUSE cell is free, then all pointers to it are invalid
	// 			shape->remove_relation(row, col, EQ);
	// 		}

	// 	}
	// }
	// success = make_concretisation(*shape);
	// if (!success) {
	// 	delete shape;
	// 	return NULL;
	// }

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

std::vector<Cfg> mk_one_interference(const Cfg& c1, const Cfg& c2) {
	// bool cond =  c1.pc[0]->id()==56 && c1.shape->test(7,9,MT) && c1.shape->test(9,5,MT) && c1.shape->test(7,5,GT) && c2.pc[0]->id()==57;
	// bool cond = c1.pc[0]->id()==26 && c1.shape->test(7,1,EQ) && c2.pc[0]->id()==30;
	// bool cond = c1.pc[0]->id()==26 && c1.shape->test(7,1,EQ) && c1.shape->test(7,5,MT) && c1.shape->test(1,5,MT) && !c1.valid_ptr.at(7) && !c1.freed && c2.pc[0]->id()==8;
	// bool cond =  c1.pc[0]->id()==26 && c1.shape->test(7,1,EQ) && c1.shape->test(7,5,MT) && c1.shape->test(1,5,MT) && !c1.valid_ptr.at(7) && c2.pc[0]->id()==9 && c2.shape->test(6,1,EQ) && c2.shape->test(7,5,EQ);
	// bool cond = false; // c1.pc[0]->id()==35 && c2.pc[0]->id()==35 && c1.inout[0].type() == OValue::ANONYMOUS && c2.inout[0].type() == OValue::ANONYMOUS;

	// this function assumes that c2 can interfere c1
	// extend c1 with (parts of) c2, compute post on the extended cfg, project away the extended part
	unsigned short extended_thread_tid = 1; // 0-indexed

	// extend c1 with c2 (we need a temporary cfg since we cannot/shouldnot modify cfgs that stored in the encoding)
	std::unique_ptr<Cfg> extended((extend_cfg(c1, c2)));
	if (!extended) return {};

	const Cfg& tmp = *extended;
	// bool cond =  tmp.pc[0]->id()==26 && !tmp.valid_ptr.at(7) && tmp.pc[1]->id()==9 && tmp.shape->test(7,8,EQ);
	// bool cond =  tmp.pc[0]->id()==26 && tmp.shape->test(7,1,EQ) && tmp.shape->test(7,5,MT) && c1.shape->test(1,5,MT) && !c1.valid_ptr.at(7) && c2.pc[0]->id()==9 && c2.shape->test(6,1,EQ) && c2.shape->test(7,5,EQ) ;
	// if (cond) {
	// 	std::cout << "===============================" << "\n";
	// 	std::cout << "Interference for: " << c1 << *c1.shape << "\n";
	// 	std::cout << "and: " << c2 << *c2.shape << "\n";
	// 	std::cout << "-------------------------------" << "\n";
	// 	std::cout << "Interference post for: " << tmp << *tmp.shape;
	// 	std::cout << "-------------------------------" << "\n";
	// }

	// do one post step for the extended thread
	INTERFERENCE_STEPS++;
	std::vector<Cfg> postcfgs = tmr::post(tmp, extended_thread_tid);

	
	// the resulting cfgs need to be projected to 1 threads, then push them to result vector
	for (Cfg& pcfg : postcfgs) {
		// bool cond1 = c1.pc[0]->id()>=56 && c2.pc[0]->id()<=57;
		// if (cond1) {
		// 	std::cout << "===============================" << "\n";
		// 	std::cout << "Interference for: " << c1 << *c1.shape << "\n";
		// 	std::cout << "and: " << c2 << *c2.shape << "\n";
		// 	std::cout << "-------------------------------" << "\n";
		// 	std::cout << "Interference post for: " << tmp << *tmp.shape << "\n";
		// 	std::cout << "Produced (before projection): " << pcfg << *pcfg.shape << "\n";
		// }

		// if (cond) std::cout << "Produced (before porjection): " << pcfg << *pcfg.shape << std::endl;
		project_away(pcfg, extended_thread_tid);
	}
	// if (cond) exit(0);

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

