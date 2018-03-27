#include "fixp/interference.hpp"

#include <stdexcept>
#include "helpers.hpp"
#include "counter.hpp"
#include "config.hpp"

using namespace tmr;


// /******************************** CHECK MATCH ********************************/

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
	return true;
}

static bool is_noop(const Statement& pc) {
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

bool can_interfere(const Cfg& cfg, const Cfg& interferer) {
	// 0. Optimizations
	#if SKIP_NOOPS
		if (is_noop(*cfg.pc[0])) return false;
		if (is_noop(*interferer.pc[0])) return false;
	#endif

	// 1. REUSE must be in the same state
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


// /******************************** EXTENSION ********************************/

Cfg* extend_cfg(const Cfg& dst, const Cfg& interferer) {
	// extend dst with interferer (create a copy of dst, then extend the copy)

	// 1.0 extend shape
	Shape* shape = new Shape(*dst.shape);	
	shape->extend();

	// some cells can be intersected in advance, have to be unified
	std::size_t end = dst.shape->offset_locals(0);
	for (std::size_t i = 0; i < end; i++) {
		for (std::size_t j = i+1; j < end; j++) {
			shape->set(i, j, intersection(dst.shape->at(i, j), interferer.shape->at(i, j)));
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
			
			if (is_row_owned && is_col_owned) {
				shape->set(dst_row, dst_col, BT_);
			} else if ((is_row_owned && is_col_valid) || (is_col_owned && is_row_valid)) {
				// ownership guarantees that other threads have only invalid pointers
				delete shape;
				return NULL;
			} else {
				shape->set(dst_row, dst_col, PRED);
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
	// seen does not need to be changed
}


// /******************************** INTERFERENCE ********************************/

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

				// bool cond = false; // c1.pc[0]->id()==26 && c1.shape->test(7,1,EQ) && c1.shape->test(7,5,MT) && c1.shape->test(1,5,MT) && !c1.valid_ptr.at(7) && !c1.freed;// && c2.shape->test(6,1,EQ) && c2.shape->test(7,5,EQ);
				// bool cond = c1.pc[0]->id()==35 && c2.pc[0]->id()==35 && c1.inout[0].type() == OValue::ANONYMOUS && c2.inout[0].type() == OValue::ANONYMOUS;
				// bool cond = false;
				// if (cond) {
				// 	std::cout << "===============================" << "\n";
				// 	std::cout << "Interference check for: " << c1 << *c1.shape << "\n";
				// 	std::cout << "and: " << c2 << *c2.shape << "\n";
				// }
				if (can_interfere(c1, c2)) {
					// if (cond) std::cout << "interference!" << std::endl;
					work.add(mk_one_interference(c1, c2));
					work.add(mk_one_interference(c2, c1));
					counter++;
				} else {
					// if (cond) std::cout << "NO interference!" << std::endl;
					// if (cond) exit(0);
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
