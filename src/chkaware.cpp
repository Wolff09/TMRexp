#include "chkaware.hpp"

#include <deque>
#include "post.hpp"
#include "helpers.hpp"
#include "config.hpp"
#include "counter.hpp"

using namespace tmr;


struct ABAinfo {
	bool aba_prone;
	std::size_t var;
	std::size_t valid_cmp;
	const Ite* ite;
	ABAinfo() : aba_prone(false), var(0), valid_cmp(0), ite(nullptr) {}
	ABAinfo(bool p, std::size_t v, std::size_t c, const Ite* i) : aba_prone(p), var(v), valid_cmp(c), ite(i) {
		if (!ite) throw std::logic_error("ABAinfo initialized with nullptr.");
	}
};

static inline ABAinfo is_aba_prone(const Cfg& cfg) {
	if (!cfg.pc[0]) return ABAinfo();
	if (cfg.pc[0]->clazz() != Statement::ITE) return ABAinfo();
	auto& stmt = static_cast<const Ite&>(*cfg.pc[0]);
	if (stmt.cond().type() != Condition::EQNEQ) return ABAinfo();
	auto& cond = static_cast<const EqNeqCondition&>(stmt.cond());
	if (cond.lhs().clazz() != Expr::VAR) return ABAinfo();
	if (cond.rhs().clazz() != Expr::VAR) return ABAinfo();
	auto& lhs = static_cast<const VarExpr&>(cond.lhs());
	auto& rhs = static_cast<const VarExpr&>(cond.rhs());
	auto lhs_var = mk_var_index(*cfg.shape, lhs, 0);
	auto rhs_var = mk_var_index(*cfg.shape, rhs, 0);
	bool lhs_valid = cfg.valid_ptr.at(lhs_var);
	bool rhs_valid = cfg.valid_ptr.at(rhs_var);
	bool lhs_local = lhs.decl().local();
	bool rhs_local = rhs.decl().local();
	if (lhs_valid && rhs_valid) return ABAinfo();
	if (lhs_valid ^ rhs_valid) {
		if (cond.is_inverted()) {
			throw std::logic_error("Unsupported ABA prone assertion: condition over '!=', must be '=='.");
		}
		if (!(lhs_local ^ rhs_local)) {
			throw std::logic_error("Unsupported ABA prone assertion: condition must contain exactly one shared pointer.");
		}
		auto var = lhs_local ? lhs_var : rhs_var;
		auto cmp = lhs_local ? rhs_var : lhs_var;
		return { true, var, cmp, &stmt };
	}
	throw std::logic_error("Unsupported ABA prone assertion: comparing two invalid pointers.");
}

static inline std::unique_ptr<Cfg> prune_reuse(const Cfg& cfg, std::vector<std::size_t> vars) {
	std::unique_ptr<Cfg> result(new Cfg(cfg.copy()));
	Shape& shape = *result->shape;
	auto re = shape.index_REUSE();
	for (auto v : vars) shape.remove_relation(v, re, EQ);
	bool success = make_concretisation(shape);
	if (!success) result.reset();
	return result;
}

static inline std::deque<Cfg> post_branch(const Cfg& cfg, const Statement* branch) {
	std::deque<Cfg> result;
	auto post = tmr::post(cfg, 0);
	for (auto& cf : post) {
		if (cf.pc[0] == branch) {
			result.push_back(std::move(cf));
		}
	}
	return result;
}

static inline std::pair<std::deque<Cfg>, std::deque<Cfg>> mk_continuations(const Ite& abaprone, const Cfg& src) {
	// gives two lists:
	//     - result.first are cfgs that retry (come back to abaprone)
	//     - result.second are cfgs that do not retry (do not come back to abaprone)

	std::deque<Cfg> worklist;
	auto nextfalse = post_branch(src, abaprone.next_false_branch());
	worklist.insert(worklist.end(), std::make_move_iterator(nextfalse.begin()), std::make_move_iterator(nextfalse.end()));

	std::deque<Cfg> retry, noretry;
	while (!worklist.empty()) {
		Cfg cfg = std::move(worklist.back());
		worklist.pop_back();

		// function returned ==> no retry
		if (!cfg.pc[0]) {
			noretry.push_back(std::move(cfg));
			continue;
		}

		// abaprone reached ==> retry
		if (cfg.pc[0] == &abaprone) {
			retry.push_back(std::move(cfg));
			continue;
		}

		// continue search for abaprone
		auto post = tmr::post(cfg, 0);
		worklist.insert(worklist.end(), std::make_move_iterator(post.begin()), std::make_move_iterator(post.end()));
	}

	return { std::move(retry), std::move(noretry) };
}

static inline bool shared_shape_inlusion(const Shape& shape, const Shape& other) {
	// return true iff. every non-local cell of shape is inlcuded in the corresponding cell of other
	for (std::size_t i = 0; i < shape.offset_locals(0); i++) {
		for (std::size_t j = i+1; j < shape.offset_locals(0); j++) {
			if (!subset(shape.at(i,j), other.at(i,j))) {
				return false;
			}
		}
	}
	return true;
}

static inline bool allowed_retry_state(const State* state, const State* other) {
	// returns true iff retrying from state and reaching other is okay
	if (state == other) return true;
	if (state != NULL && other != NULL) {
		if (state->name() == "d" && other->name() == "s0") return true;
		if (state->name() == "dg" && other->name() == "g") return true;
	}
	return false;
}

static inline void chk_retry(const std::deque<Cfg>& retry, const Cfg& aba, std::size_t var, const Encoding& enc) {
	for (const Cfg& cfg : retry) {
		bool failed = false;
		std::string reason = "";
		#define FAIL(x) { failed = true; reason += x; }

		// check shape
		if (!shared_shape_inlusion(*cfg.shape, *aba.shape)) {
			FAIL("Failed to match non-local shape.")
		}

		// check non-local flags
		if (aba.freed != cfg.freed) {
			FAIL("Failed to match 'freed' .")
		}
		if (aba.retired != cfg.retired) {
			FAIL("Failed to match 'retired' .")
		}
		if (aba.state != cfg.state) {
			FAIL("Failed to match 'state' .")
		}
		if (aba.seen != cfg.seen) {
			FAIL("Failed to match 'seen' .")
		}
		if (aba.inout[0] != cfg.inout[0]) {
			FAIL("Failed to match 'inout' .")
		}
		if (aba.oracle != cfg.oracle) {
			FAIL("Failed to match 'oracle' .")
		}
		if (aba.own != cfg.own) {
			FAIL("Failed to match 'own' .")
		}

		// check local pointer info
		for (std::size_t i = cfg.shape->offset_locals(0); i < cfg.shape->size(); i++) {
			if (i == var) continue;
			if (aba.valid_ptr.at(i) != cfg.valid_ptr.at(i)) {
				FAIL("Failed to match 'valid_ptr' on variable not participating in ABA prone comparison. ");
			}
			if (aba.valid_next.at(i) != cfg.valid_next.at(i)) {
				FAIL("Failed to match 'valid_next' on variable not participating in ABA prone comparison. ");
			}
			if (aba.guard0state.at(i) != cfg.guard0state.at(i)) {
				FAIL("Failed to match 'guard0state' on variable not participating in ABA prone comparison. ");
			}
			if (aba.guard1state.at(i) != cfg.guard1state.at(i)) {
				FAIL("Failed to match 'guard1state' on variable not participating in ABA prone comparison. ");
			}
		}
		if (!cfg.valid_ptr.at(var)) {
			FAIL("Failed to match 'valid_ptr' on variable participating in ABA prone comparison. ");
		}
		if (!cfg.valid_next.at(var)) {
			FAIL("Failed to match 'valid_next' on variable participating in ABA prone comparison. ");
		}
		// Note: smr state can change due to comparison with shared pointer variable and re-read
		if (!allowed_retry_state(aba.guard0state.at(var), cfg.guard0state.at(var))) {
			FAIL("Failed to match 'guard0state' on variable participating in ABA prone comparison. ");
		}
		if (!allowed_retry_state(aba.guard1state.at(var), cfg.guard1state.at(var))) {
			FAIL("Failed to match 'guard1state' on variable participating in ABA prone comparison. ");
		}

		if (failed) {
			std::cout << std::endl << std::endl;
			std::cout << "Malicious ABA: retrying configuration failed." << std::endl;
			std::cout << "Starting from ABA cfg:   " << aba << * aba.shape << std::endl;
			std::cout << "Ending in retry cfg: " << cfg << *cfg.shape << std::endl;
			std::cout << reason << std::endl;
			throw std::logic_error("Malicious ABA: retrying configuration failed.");
		}
	}
}

static inline void chk_noretry(const std::deque<Cfg>& noretry, const Cfg& aba, const Encoding& enc) {
	const Statement* ite = aba.pc[0];

	// compute shapes for aba true case
	// Note: enc can contain configurations that are "like" cfg on shared heap
	// That does not mean that any post image of the ite in aba allows this.
	// (This is because the post image can be modified by interference, what it is not visible in enc.)
	// Hence, we need to consider every configuration at ite, do a post, and check if it is "like" cfg.
	std::vector<Shape*> shapes_to_merge;
	for (auto& kvp : enc) {
		for (const Cfg& ec : kvp.second) {
			if (ec.pc[0] != ite) continue;
			auto posttrue = post_branch(ec, 0);
			for (auto& pcf : posttrue) {
				shapes_to_merge.push_back(new Shape(*pcf.shape));
			}
		}
	}

	// ensure that no-retry shapes cannot be contained in the above true shapes
	Shape* shape = tmr::merge(shapes_to_merge);
	if (!shape) return;

	for (const Cfg& cf : noretry) {
		// only configurations that cannot take the true branch at aba are allowed to "escape" (be non-retryers)
		if (shared_shape_inlusion(*cf.shape, *shape)) {
			std::cout << std::endl << std::endl;
			std::cout << "Malicious ABA: non-retrying configuration failed." << std::endl;
			std::cout << "Starting from ABA cfg:   " << aba << * aba.shape << std::endl;
			std::cout << "Ending in non-retry cfg: " << cf << *cf.shape << std::endl;
			std::cout << "Failed to mismatch merged shape: " << std::endl << *shape << std::endl;
			throw std::logic_error("Malicious ABA: non-retrying configuration failed.");
		}
	}
}

bool tmr::chk_aba_awareness(const Encoding& enc) {
	std::size_t aba_count = 0;

	for (auto& kvp : enc) {
		for (auto it = kvp.second.begin(); it != kvp.second.end(); it++) {
			const Cfg& cfg = *it;
			
			auto info = is_aba_prone(cfg);
			if (!info.aba_prone) continue;

			auto noreuse = prune_reuse(cfg, { info.var, info.valid_cmp });
			if (!noreuse) continue;

			const Cfg& aba = *noreuse;
			auto continuations = mk_continuations(*info.ite, aba);

			// TODO: ensure that the shared state was not changed when computing the continuations cfgs.

			chk_retry(continuations.first, aba, info.var, enc);
			chk_noretry(continuations.second, aba, enc);

			aba_count++;
		}
	}

	std::cerr << "#ABA = " << aba_count << std::endl;

	return true;
}
