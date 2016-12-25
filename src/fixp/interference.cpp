#include "fixp/interference.hpp"

#include <stdexcept>
#include "helpers.hpp"
#include "counter.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** CHECK MATCH ********************************/

bool is_observed_owned(const Cfg& cfg, std::size_t obs) {
	std::size_t begin = cfg.shape->offset_locals(0);
	std::size_t end = begin + cfg.shape->sizeLocals();
	for (std::size_t i = begin; i < end; i++)
		if (cfg.own.is_owned(i) && cfg.shape->test(obs, i, EQ))
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

	for (std::size_t i = 0; i < end; i++) {
		if (i == 3 || i == 4) continue;
		for (std::size_t j = i+1; j < end; j++) {
			if (j == 3 || j == 4) continue;
			if (intersection(cfg.shape->at(i, j), interferer.shape->at(i, j)).none())
				return false;
		}
	}

	for (std::size_t i : {3, 4}) {
		if (cfg.shape->at(i, cfg.shape->index_UNDEF()) == MT_ ^ interferer.shape->at(i, interferer.shape->index_UNDEF()) == MT_) {
			return false;
		}

		bool is_global = is_observed_global(cfg, i) || is_observed_global(interferer, i);
		if (!is_global) continue;


		bool is_owned = is_observed_owned(cfg, i) || is_observed_owned(interferer, i);
		if (is_owned) continue;

		for (std::size_t j = 0; j < end; j++)
			if (intersection(cfg.shape->at(i, j), interferer.shape->at(i, j)).none())
				return false;
	}

	return true;
}

static bool is_noop(const Statement& pc) {
	switch (pc.clazz()) {
		case Statement::SQZ:     return true;
		case Statement::WHILE:   return true;
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
			if (cfg.inout[0].type() != OValue::OBSERVABLE) {
				set_tmp(static_cast<const ReadInputAssignment&>(pc).expr());
				break;
			}
			NO_SKIP;
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

static bool can_skip_victim(const Cfg& cfg) {
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
			// TODO: if (victim && cont lhs and rhs are local) SKIP ??????;
			NO_SKIP;
		case Statement::INPUT:
			set_tmp(static_cast<const ReadInputAssignment&>(pc).expr());
			break;
		case Statement::SETNULL:
			set_tmp(static_cast<const NullAssignment&>(pc).lhs());
			break;
		default:
			NO_SKIP;
	}

	if (cfg.own.is_owned(tmp)) SKIP;
	NO_SKIP;
}

bool can_interfere(const Cfg& cfg, const Cfg& interferer) {
	#if SKIP_NOOPS
		if (is_noop(*cfg.pc[0])) return false;
		if (is_noop(*interferer.pc[0])) return false;
	#endif

	#if INTERFERENCE_OPTIMIZATION
		if (can_skip_victim(cfg)) return false;
		if (can_skip(interferer)) return false;
	#endif

	// 1. cfg.state must match interferer.state
	//    => already done in tmr::mk_all_interference
	if (!(cfg.state == interferer.state))
		return false;

	// 2. prevent some data combinations which are excluded by the data independence argument
	if (cfg.inout[0] == interferer.inout[0])
		if (cfg.inout[0].type() != OValue::DUMMY)
				return false;

	// 3. interfering thread should not be behind or ahead (too far) of seen
	//    A thread can be ahead of the values which have been added and removed from the data structure
	//    if they are about to add one.
	//    So we just ensure that two "out"-threads have seen the same values.
	//    For everything else, we rely on the later shape comparison (if a value was seen
	//    it is not in relation with UNDEF, otherwise it is).
	if (cfg.seen != interferer.seen && !interferer.pc[0]->function().has_input())
		return false;

	// 4. cfg.shape must be a superset of interferer.shape on the non-local cell terms
	if (!do_shapes_match(cfg, interferer))
		return false;

	return true;
}


/******************************** EXTENSION ********************************/

Cfg* extend_cfg(const Cfg& dst, const Cfg& interferer, unsigned short extended_tid) {
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
			for (std::size_t j = 0; j < end; j++) {
				shape->set(i, j, setunion(dst.shape->at(i, j), interferer.shape->at(i, j)));
			}
		}
	}

	// 1.1 extend shape: add locals of interfering thread
	assert(shape->size() == dst.shape->size() + shape->sizeLocals());
	for (std::size_t i = 0; i < interferer.shape->sizeLocals(); i++) {
		std::size_t src_col = interferer.shape->offset_locals(0) + i;
		std::size_t dst_col = dst.shape->size() + i;
		
		// add extended.locals ~ extended.locals
		for (std::size_t j = i; j < interferer.shape->sizeLocals(); j++) {
			std::size_t src_row = interferer.shape->offset_locals(0) + j;
			std::size_t dst_row = dst.shape->size() + j;
			shape->set(dst_row, dst_col, interferer.shape->at(src_row, src_col));
		}

		// add specials/non-locals/MM-tid0-locals ~ extended.locals
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
			// according to ownership
			bool is_row_owned = false;
			bool is_col_owned = false;

			is_row_owned = dst.own.is_owned(src_row);
			is_col_owned = interferer.own.is_owned(src_col);
			
			if (is_row_owned && is_col_owned)
				shape->set(dst_row, dst_col, BT_);
			else if (is_row_owned)
				shape->set(dst_row, dst_col, MT_GT_BT);
			else if (is_col_owned)
				shape->set(dst_row, dst_col, MF_GF_BT);
		}
	}

	// if (msetup == PRF) {
	// 	for (std::size_t i = 0; i < shape->sizeLocals(); i++)
	// 		for (std::size_t j = 0; j < shape->sizeLocals(); j++) {
	// 			std::size_t row = shape->offset_locals(0) + i;
	// 			std::size_t src_col = shape->offset_locals(0) + j;
	// 			std::size_t col = shape->offset_locals(extended_tid) + j;
	// 			if (shape->test(row, shape->index_FREE(), MT))
	// 				if (interferer.own.is_owned(src_col) && interferer.shape->at(src_col, shape->index_UNDEF()) != MT_) {
	// 					for (std::size_t t = 0; t < shape->size(); t++)
	// 						shape->set(row, t, setunion(shape->at(row, t), shape->at(col, t)));
	// 					shape->set(row, row, EQ_);
	// 					shape->set(row, shape->index_UNDEF(), BT_);
	// 				}
	// 		}
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
	assert(consistent(*shape));

	// 2. extend inout
	Cfg* result = new Cfg(dst, shape);
	Cfg& res = *result;
	res.inout[extended_tid] = interferer.inout[0];

	// 3. extend pc
	assert(interferer.pc[0] != NULL);
	res.pc[extended_tid] = interferer.pc[0];

	// 4. nothing to do for seen
	for (std::size_t i = 0; i < res.seen.size(); i++)
		res.seen[i] = dst.seen[i] | interferer.seen[i];

	// 5. nothing to do for state

	// 6. ownership
	for (std::size_t i = 0; i < dst.shape->sizeLocals(); i++) {
		res.own.set_ownership(dst.shape->size() + i, interferer.own.is_owned(dst.shape->offset_locals(0) + i));
	}

	// 7. extend oracle
	res.oracle[extended_tid] = interferer.oracle[0];

	return result;
}


/******************************** PROJECTION ********************************/

static inline void project_away(Cfg& cfg, unsigned short extended_thread_tid) {
	assert(extended_thread_tid == 1);

	// reset ownership to dummy value
	for (std::size_t i = cfg.shape->offset_locals(extended_thread_tid); i < cfg.shape->size(); i++) {
		cfg.own.publish(i);
	}

	// remove extended thread from cfg
	cfg.shape->shrink();

	cfg.pc[extended_thread_tid] = NULL;
	cfg.inout[extended_thread_tid] = OValue();
	cfg.oracle[extended_thread_tid] = false;

	// the state does not need to be changed

	// seen does not need to be changed ~> if the extended thread add a value to seen, then
	// only he can add this value to the datastructure (by data independence argument) and
	// thus we do not want to remove anything from there
}


/******************************** INTERFERENCE FOR TID ********************************/

std::vector<Cfg> mk_one_interference(const Cfg& c1, const Cfg& c2) {
	// this function assumes that c2 can interfere c1
	// extend c1 with c2 (we need a temporary cfg since we cannot/shouldnot modify cfgs that stored in the encoding)
	std::unique_ptr<Cfg> extended((extend_cfg(c1, c2, 1)));
	if (!extended) return {};

	const Cfg& tmp = *extended;
	assert(tmp.shape != NULL);

	// do one post step for the extended thread
	assert(tmp.pc[1] != NULL);
	INTERFERENCE_STEPS++;
	std::vector<Cfg> postcfgs = tmr::post(tmp, 1);
	
	// the resulting cfgs need to be projected to 1/2 threads, then push them to result vector
	for (Cfg& pcfg : postcfgs) {
		project_away(pcfg, 1);
	}

	return postcfgs;
}


/******************************** INTERFERENCE FOR ALL THREADS ********************************/

#if REPLACE_INTERFERENCE_WITH_SUMMARY
	static inline std::vector<OValue> get_possible_ovaluess(const Cfg& cfg, const Observer& observer, const Function& fun) {
		assert(fun.has_input() ^ fun.has_output());
		std::vector<OValue> result;
		if (fun.has_input()) {
			for (std::size_t i = 0; i < observer.numVars(); i++)
				if (!cfg.seen[i])
					result.push_back(observer.mk_var(i));
			result.push_back(OValue::Anonymous());
		} else {
			result.push_back(OValue());
		}
		return result;
	}

	static inline bool is_var_eq_null_ite(const Ite& ite) {
		if (ite.cond().type() == Condition::EQNEQ) {
			auto& cond = static_cast<const EqNeqCondition&>(ite.cond());
			if (cond.rhs().clazz() == Expr::NIL && cond.lhs().clazz() == Expr::VAR)
				if (static_cast<const VarExpr&>(cond.lhs()).decl().local())
					return true;
		}
		return false;
	}

	static bool can_skip_summary(const Cfg& cfg) {
		#define SKIP {INTERFERENCE_SKIPPED++; return true;}
		#define NO_SKIP {return false;}

		if (!cfg.pc[0])
			SKIP;

		const Statement& stmt = *cfg.pc[0];
		
		if (is_noop(stmt))
			SKIP;

		#define SKIP_IF_OWNED(E) {if(cfg.own.is_owned(mk_var_index(*cfg.shape, E, 0)))SKIP;}

		switch (stmt.clazz()) {

			case Statement::SETNULL:
				SKIP_IF_OWNED(static_cast<const NullAssignment&>(stmt).lhs());
				break;

			case Statement::INPUT:
				SKIP_IF_OWNED(static_cast<const ReadInputAssignment&>(stmt).expr());
				break;

			case Statement::FREE:
				SKIP;
				break;

			case Statement::ITE:
				if (is_var_eq_null_ite(static_cast<const Ite&>(stmt)))
					SKIP;
				break;

			default:
				break;
		}

		NO_SKIP;
	}
#endif

void tmr::mk_summary(RemainingWork& work, const Cfg& cfg, const Program& prog) {
	#if !REPLACE_INTERFERENCE_WITH_SUMMARY
		throw std::logic_error("Cannot apply summaries in interferecne-mode.");
	#else
		#if SUMMARY_OPTIMIZATION
			if (can_skip_summary(cfg))
				return;
		#endif

		Cfg tmp = cfg.copy();
		tmp.shape->extend();
		auto tmp_seen = tmp.seen;

		// remove local bindings that may be present in the shape (for whatever reason)
		for (std::size_t i = tmp.shape->offset_locals(1); i < tmp.shape->size(); i++) {
			for (std::size_t t = 0; t < tmp.shape->size(); t++) {
				tmp.shape->set(i, t, BT);
			}
			tmp.shape->set(i, i, EQ);
			tmp.shape->set(i, tmp.shape->index_UNDEF(), MT);
		}

		// execute summaries
		for (std::size_t i = 0; i < prog.size(); i++) {
			auto& fun = prog.at(i);
			auto& sum = fun.summary();
			std::vector<OValue> ovals = get_possible_ovaluess(cfg, cfg.state.observer(), fun);

			for (OValue ov : ovals) {
				tmp.pc[1] = &sum;
				tmp.inout[1] = ov;
				tmp.seen = tmp_seen;
				if (ov.type() == OValue::OBSERVABLE)
					tmp.seen[ov.id()] = true;

				INTERFERENCE_STEPS++;
				auto postcfgs = tmr::post(tmp, 1);
				for (auto& c : postcfgs) project_away(c, 1);
				work.add(std::move(postcfgs));
			}
		}

	#endif
}

static inline const Program& getprog(const Encoding::__sub__store__& region) {
	for (const auto& cfg : region) {
		if (cfg.pc[0]) return cfg.pc[0]->function().prog();
	}
	throw std::runtime_error("Bucket does not contain any Program pointer :(");
}

void mk_regional_interference(RemainingWork& work, Encoding::__sub__store__& region, std::size_t& counter) {
	// begin = first cfg
	// end = post last cfg

	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		auto& prog = getprog(region);
	#endif

	std::size_t old_size;
	do {
		old_size = region.size();
		for (auto it1 = region.begin(); it1 != region.end(); it1++) {
			const Cfg& c1 = *it1;
			// if (c1.pc[0] == NULL) continue;

			#if REPLACE_INTERFERENCE_WITH_SUMMARY

				auto worker_size = work.size();
				mk_summary(work, c1, prog);
				counter += work.size() - worker_size;

			#else
				
				if (c1.pc[0] == NULL) continue;

				for (auto it2 = it1; it2 != region.end(); it2++) {
					const Cfg& c2 = *it2;
					if (c2.pc[0] == NULL) continue;

					if (can_interfere(c1, c2)) {
						work.add(mk_one_interference(c1, c2));
						counter++;
					}
					if (can_interfere(c2, c1)) {
						work.add(mk_one_interference(c2, c1));
						counter++;
					}
				}

			#endif
		}
	} while
	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		(false);
	#else
		(old_size < region.size());
	#endif
}

void tmr::mk_all_interference(Encoding& enc, RemainingWork& work) {	
		std::cerr << "interference...   ";
		std::size_t counter = 0;

		for (auto& kvp : enc) {
			std::cerr << "[" << kvp.second.size() << "-" << enc.size()/1000 << "k]";
			mk_regional_interference(work, kvp.second, counter);
		}

		std::cerr << " done! [enc.size()=" << enc.size() << ", matches=" << counter << "]" << std::endl;
}
