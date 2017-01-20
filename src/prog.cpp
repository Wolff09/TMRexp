#include "prog.hpp"

#include <stdexcept>
#include <set>
#include "make_unique.hpp"
#include "config.hpp"

using namespace tmr;


/******************************** CONSTRUCTION ********************************/

static std::vector<std::unique_ptr<Variable>> mk_vars(bool global, std::vector<std::string> names) {
	std::vector<std::unique_ptr<Variable>> result(names.size());
	for (std::size_t i = 0; i < result.size(); i++) {
		assert(names[i] != "__in__");
		assert(names[i] != "__out__");
		result[i] = std::make_unique<Variable>(names[i], i, global);
	}
	return result;
}

static bool has_init_name_clash(const std::vector<std::unique_ptr<Function>>& funs) {
	for (const auto& f : funs)
		if (f->name() == "init")
			return true;
	return false;
}

struct VariableComparator {
	bool operator()(const Variable& lhs, const Variable& rhs) const {
		return &lhs < &rhs;
	}
};

static void enforce_static_properties(const Statement* stmtptr,
                                      std::map<std::reference_wrapper<const Variable>, std::reference_wrapper<const Expr>, VariableComparator> var2val,
                                      std::set<std::reference_wrapper<const Variable>, VariableComparator> allocations,
                                      std::set<std::reference_wrapper<const Variable>, VariableComparator> tobefreed,
                                      std::size_t number_of_seen_cas) {
	// we enforce statically:
	// (0) static single assignment form
	// (1) at most one CAS path in summary
	// (2) cells that are removed from the global state are freed
	//     we do this as follows:
	//         - using (0)
	//         - content only removed when "out" linearisation point fired
	//         - ensure that this is done in CAS equivalent to "p = p.next"
	//         - ensure that old p is captured by some local pointer variable
	//         - ensure that this local is freed
	// (3) allocations are published
	//     we do this as follows:
	//         - using (0)
	//         - ensuring that an allocation appears as src in some CAS


	auto ensure_not_assigned = [&] (const Variable& var) {
		if (allocations.find(var) != allocations.end() || var2val.find(var) != var2val.end())
			throw std::logic_error("Bad summary: multiple assignments to variables in summareis are not supported.");
	};

	if (stmtptr == NULL) {
		if (tobefreed.size() != 0) throw std::logic_error("Bad summary: some cells need freeing.");
		if (allocations.size() != 0) throw std::logic_error("Bad summary: some cells need publishing.");
		return;
	}
	const Statement& stmt = *stmtptr;

	if (stmt.clazz() == Statement::WHILE) {
		throw std::logic_error("Bad summary: while loops not supported.");
	} else if (stmt.clazz() == Statement::ITE) {
		const Ite& ite = static_cast<const Ite&>(stmt);
		auto condtype = ite.cond().type();
		if (condtype == Condition::CASC || condtype == Condition::COMPOUND || condtype == Condition::ORACLEC)
			throw std::logic_error("Bad summary: unsupported condition.");
		if (ite.next_false_branch()) enforce_static_properties(ite.next_false_branch(), var2val, allocations, tobefreed, number_of_seen_cas);
		if (ite.next_true_branch()) enforce_static_properties(ite.next_true_branch(), std::move(var2val), std::move(allocations), std::move(tobefreed), number_of_seen_cas);
		return;
	} else if (stmt.clazz() == Statement::ATOMIC) {
		enforce_static_properties(&static_cast<const Atomic&>(stmt).sqz(), std::move(var2val), std::move(allocations), std::move(tobefreed), number_of_seen_cas);
		return;
	} else if (stmt.clazz() == Statement::ASSIGN || stmt.clazz() == Statement::SETNULL) {
		auto& lhsexpr = stmt.clazz() == Statement::ASSIGN ? static_cast<const Assignment&>(stmt).lhs() : static_cast<const NullAssignment&>(stmt).lhs();
		auto& rhsexpr = stmt.clazz() == Statement::ASSIGN ? static_cast<const Assignment&>(stmt).rhs() : *std::make_shared<NullExpr>();
		if (lhsexpr.clazz() == Expr::VAR) {
			const Variable& var = static_cast<const VarExpr&>(lhsexpr).decl();
			ensure_not_assigned(var);
			if (!var2val.insert({ var, rhsexpr }).second)
				throw std::logic_error("Bad summary: malicious insertion to var2val.");
		}
	} else if (stmt.clazz() == Statement::MALLOC) {
		if (!allocations.insert(static_cast<const Malloc&>(stmt).decl()).second)
			throw std::logic_error("Bad summary: malicious insertion to allocations.");
	} else if (stmt.clazz() == Statement::FREE) {
		const Variable& trg = static_cast<const Malloc&>(stmt).decl();
		tobefreed.erase(trg);
		// TODO: remove aliases from tobefreed
	} else if (stmt.clazz() == Statement::CAS) {
		// if (number_of_seen_cas != 0) throw std::logic_error("Bad summary: too many CAS at current path.");
		number_of_seen_cas++;
		const CompareAndSwap& cas = static_cast<const CompareAndSwap&>(stmt);
		if (cas.fires_lp() && cas.lp().event().has_output()) {
			// ensure that the effect of the CAS is dst = dst.next
			if (cas.dst().clazz() != Expr::VAR) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (dst is no var).");
			const Variable& dst = static_cast<const VarExpr&>(cas.dst()).decl();
			if (cas.cmp().clazz() != Expr::VAR) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (cmp is no var).");
			const Variable& cmp = static_cast<const VarExpr&>(cas.cmp()).decl();
			if (&dst != &cmp) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (dst and cmp mismatch).");
			if (cas.src().clazz() != Expr::VAR) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (src is no var).");
			const Variable& src = static_cast<const VarExpr&>(cas.src()).decl();
			if (var2val.find(src) == var2val.end()) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (src does not have a value).");
			const Expr& srcval = var2val.at(src);
			if (srcval.clazz() != Expr::SEL) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (srcval is no selector).");
			const Variable& evalsrc = static_cast<const Selector&>(srcval).decl();
			if (&dst != &evalsrc) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (bad cas assignment).");
			// add pre-CAS dst to tobefreed
			bool carbon_copy_found = false;
			for (const auto& entry : var2val) {
				const Expr& value = entry.second;
				if (value.clazz() == Expr::VAR) {
					const Variable& var = static_cast<const VarExpr&>(value).decl();
					if (&var == &dst) {
						tobefreed.insert(entry.first);
						carbon_copy_found = true;
					}
				}
			}
//			if (!carbon_copy_found) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (no alias of removed cell found).");
		} else if (cas.fires_lp() && cas.lp().event().has_input()) {
			if (cas.src().clazz() != Expr::VAR) throw std::logic_error("Bad summary: malformed CAS modifying shared heap (src is no var).");
			const Variable& src = static_cast<const VarExpr&>(cas.src()).decl();
			// remove src from allocations
			allocations.erase(src);
		}
	}

	enforce_static_properties(stmt.next(), std::move(var2val), std::move(allocations), std::move(tobefreed), number_of_seen_cas);
}

static void enforce_static_properties(const Atomic* summary) {
	enforce_static_properties(summary, {}, {}, {}, 0);
}

// static void checkLocalSSA(const Statement& stmt, std::set<std::reference_wrapper<const Variable>, VariableComparator> previous_assignments = {}) {
// 	if (stmt.clazz() == Statement::WHILE) {
// 		throw std::logic_error("While loops in summaries are not supported.");
// 	} else if (stmt.clazz() == Statement::ITE) {
// 		const Ite& ite = static_cast<const Ite&>(stmt);
// 		auto condtype = ite.cond().type();
// 		if (condtype == Condition::CASC || condtype == Condition::COMPOUND || condtype == Condition::ORACLEC)
// 			throw std::logic_error("Unsupported condition in summary.");
// 		if (ite.next_false_branch()) checkLocalSSA(*ite.next_false_branch(), previous_assignments);
// 		if (ite.next_true_branch()) checkLocalSSA(*ite.next_true_branch(), std::move(previous_assignments));
// 		return;
// 	} else if (stmt.clazz() == Statement::ATOMIC) {
// 		checkLocalSSA(static_cast<const Atomic&>(stmt).sqz(), std::move(previous_assignments));
// 		return;
// 	} else if (stmt.clazz() == Statement::ASSIGN) {
// 		const Assignment& assign = static_cast<const Assignment&>(stmt);
// 		if (assign.lhs().clazz() == Expr::VAR) {
// 			const Variable& var = static_cast<const VarExpr&>(assign.lhs()).decl();
// 			if (!previous_assignments.insert(var).second) {
// 				throw std::logic_error("Multiple assignments to local variables in summaries are not supported");
// 			}
// 		}
// 	} else if (stmt.clazz() == Statement::SETNULL) {
// 		const NullAssignment& nulla = static_cast<const NullAssignment&>(stmt);
// 		if (nulla.lhs().clazz() == Expr::VAR) {
// 			const Variable& var = static_cast<const VarExpr&>(nulla.lhs()).decl();
// 			if (!previous_assignments.insert(var).second) {
// 				throw std::logic_error("Multiple assignments to local variables in summaries are not supported");
// 			}
// 		}
// 	}
// 	auto next = stmt.next();
// 	if (next) checkLocalSSA(*next, std::move(previous_assignments));
// }

Program::Program(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::vector<std::unique_ptr<Function>> funs)
    : Program(name, std::move(globals), std::move(locals), std::make_unique<Sequence>(std::vector<std::unique_ptr<Statement>>()), std::move(funs)) {}

Program::Program(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::unique_ptr<Sequence> init, std::vector<std::unique_ptr<Function>> funs)
	: _name(name), _globals(mk_vars(true, globals)), _locals(mk_vars(false, locals)), _funs(std::move(funs)), _free(new Function("free", true, Sqz(), AtomicSqz())), _init_fun(new Function("init_dummy", true, std::move(init), AtomicSqz())) {

	assert(!has_init_name_clash(_funs));

	bool has_aux = false;
	for (const auto& n : globals) {
		if (n == "aux") {
			has_aux = true;
			break;
		}
	}
	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		for (const auto& f : _funs)
			if (!f->_summary)
				throw std::logic_error("Missing Summary for function '" + f->name() + "'!");
	#endif

	// TODO: prevent name clashes with __in__ and __out__
	// namecheck, typecheck
	std::map<std::string, Variable*> name2decl;
	for (const auto& v : _globals) {
		assert(name2decl.count(v->name()) == 0);
		name2decl[v->name()] = v.get();
	}

	// init must only access global variables
	_init_fun->namecheck(name2decl);

	for (const auto& v : _locals) {
		assert(name2decl.count(v->name()) == 0);
		name2decl[v->name()] = v.get();
	}

	for (const auto& f : _funs) {
		f->namecheck(name2decl);
		#if REPLACE_INTERFERENCE_WITH_SUMMARY
			f->_summary->namecheck(name2decl);
		#endif
	}
	for (const auto& f : _funs) f->_prog = this;
	_init_fun->_prog = this;

	// set variable ids
	for (std::size_t i = 0; i < _globals.size(); i++) _globals[i]->_id = i;
	for (std::size_t i = 0; i < _locals.size(); i++) _locals[i]->_id = i;

	// propagate stmt next field
	for (const auto& f : _funs) f->propagateNext();
	_init_fun->propagateNext();

	// propagate statement ids
	std::size_t id = 1; // id=0 is reserved for NULL
	id = _init_fun->propagateId(id);
	for (const auto& f : _funs)
		id = f->propagateId(id);
	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		for (const auto& f : _funs)
			id = f->_summary->propagateId(id);
	#endif
	_idSize = id;

	// ensure that init does only consist of simple statements
	const Statement* fi = _init();
	while (fi != NULL) {
		assert(!fi->is_conditional());
		fi = fi->next();
	}

	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		for (const auto& f : _funs)
			enforce_static_properties(f->_summary.get());
	#endif
}

bool Program::is_summary_statement(const Statement& stmt) const {
	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		return stmt.id() >= _funs.front()->_summary->id();
	#else
		return false;
	#endif
}

Function::Function(std::string name, bool is_void, std::unique_ptr<Sequence> stmts)
	: Function(name, is_void, std::move(stmts), {}) {}

Function::Function(std::string name, bool is_void, std::unique_ptr<Sequence> stmts, std::unique_ptr<Atomic> summary)
	: _name(name), _stmts(std::move(stmts)), _has_input(is_void), _summary(std::move(summary)) {
		_stmts->propagateFun(this);
		#if REPLACE_INTERFERENCE_WITH_SUMMARY
			if (!_summary) throw std::logic_error("Missing Summary for function '" + name + "'");
			_summary->propagateNext(NULL, NULL);
			_summary->propagateFun(this);
		#endif
}

Assignment::Assignment(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) : _lhs(std::move(lhs)), _rhs(std::move(rhs)), _fires_lp(false) {
	assert(_lhs->clazz() != Expr::NIL);
	assert(_rhs->clazz() != Expr::NIL);
}

Assignment::Assignment(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs, std::unique_ptr<LinearizationPoint> lp) : Assignment(std::move(lhs), std::move(rhs)) {
	_lp = std::move(lp);
	_fires_lp = true;
}

CompareAndSwap::CompareAndSwap(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, bool update_age_fields)
	: _dst(std::move(dst)), _cmp(std::move(cmp)), _src(std::move(src)), _update_age_fields(update_age_fields) {
	assert(_dst->clazz() == Expr::VAR || _dst->clazz() == Expr::SEL);
	assert(_cmp->clazz() == Expr::VAR || _cmp->clazz() == Expr::NIL);
	assert(_src->clazz() == Expr::VAR || _src->clazz() == Expr::SEL);
}

CompareAndSwap::CompareAndSwap(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, std::unique_ptr<LinearizationPoint> lp, bool update_age_fields)
	: CompareAndSwap(std::move(dst), std::move(cmp), std::move(src), update_age_fields) {
	assert(_dst->clazz() == Expr::VAR || _dst->clazz() == Expr::SEL);
	assert(_cmp->clazz() == Expr::VAR || _cmp->clazz() == Expr::NIL);
	assert(_src->clazz() == Expr::VAR || _src->clazz() == Expr::SEL);
	_lp = std::move(lp);
	assert(!_lp->has_cond());
}

CompoundCondition::CompoundCondition(std::unique_ptr<Condition> lhs, std::unique_ptr<Condition> rhs) : _lhs(std::move(lhs)), _rhs(std::move(rhs)) {
	assert(_lhs->type() != COMPOUND);
}

LinearizationPoint::LinearizationPoint() {}

LinearizationPoint::LinearizationPoint(std::unique_ptr<Condition> cond) : _cond(std::move(cond)) {}

LinearizationPoint::LinearizationPoint(std::unique_ptr<VarExpr> var) : _var(std::move(var)) {}

LinearizationPoint::LinearizationPoint(std::unique_ptr<VarExpr> var, std::unique_ptr<Condition> cond) : _var(std::move(var)), _cond(std::move(cond)) {}


/******************************** SHORTCUTS ********************************/

std::unique_ptr<VarExpr> tmr::Var (std::string name) {
	std::unique_ptr<VarExpr> res(new VarExpr(name));
	return res;
}

std::unique_ptr<Selector> tmr::Next(std::string name) {
	std::unique_ptr<Selector> res(new Selector(Var(name), POINTER));
	return res;
}

std::unique_ptr<Selector> tmr::Data(std::string name) {
	std::unique_ptr<Selector> res(new Selector(Var(name), DATA));
	return res;
}

std::unique_ptr<NullExpr> tmr::Null() {
	std::unique_ptr<NullExpr> res(new NullExpr());
	return res;
}


std::unique_ptr<EqNeqCondition> tmr::EqCond(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
	std::unique_ptr<EqNeqCondition> res(new EqNeqCondition(std::move(lhs), std::move(rhs), false));
	return res;
}

std::unique_ptr<EqNeqCondition> tmr::NeqCond(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
	std::unique_ptr<EqNeqCondition> res(new EqNeqCondition(std::move(lhs), std::move(rhs), true));
	return res;
}

std::unique_ptr<EqPtrAgeCondition> tmr::EqCondWAge(std::unique_ptr<VarExpr> lhs, std::unique_ptr<VarExpr> rhs) {
	std::unique_ptr<EqPtrAgeCondition> res(new EqPtrAgeCondition(std::move(lhs), std::move(rhs)));
	return res;
}

std::unique_ptr<CASCondition> tmr::CasCond(std::unique_ptr<CompareAndSwap> cas) {
	std::unique_ptr<CASCondition> res(new CASCondition(std::move(cas)));
	return res;
}

std::unique_ptr<NonDetCondition> tmr::NDCond() {
	std::unique_ptr<NonDetCondition> res(new NonDetCondition());
	return res;
}

std::unique_ptr<Condition> tmr::EqCond(std::unique_ptr<VarExpr> lhs, std::unique_ptr<VarExpr> rhs, bool use_age_fields) {
	if (use_age_fields) return EqCondWAge(std::move(lhs), std::move(rhs));
	else return EqCond(std::move(lhs), std::move(rhs));
}

std::unique_ptr<CompoundCondition> tmr::CompCond(std::unique_ptr<Condition> lhs, std::unique_ptr<Condition> rhs) {
	assert(lhs->type() != Condition::CASC);
	assert(rhs->type() != Condition::CASC);
	std::unique_ptr<CompoundCondition> res(new CompoundCondition(std::move(lhs), std::move(rhs)));
	return res;
}

std::unique_ptr<OracleCondition> tmr::OCond() {
	std::unique_ptr<OracleCondition> res(new OracleCondition());
	return res;
}


std::unique_ptr<Assignment> tmr::Assign (std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
	if (rhs->clazz() == Expr::NIL) throw std::logic_error("Assigning NULL not supported. Use NullAssignment/SetNull instead.");
	std::unique_ptr<Assignment> res(new Assignment(std::move(lhs), std::move(rhs)));
	return res;
}

std::unique_ptr<Assignment> tmr::Assign (std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs, std::unique_ptr<LinearizationPoint> lp) {
	if (rhs->clazz() == Expr::NIL) throw std::logic_error("Assigning NULL not supported. Use NullAssignment/SetNull instead.");
	std::unique_ptr<Assignment> res(new Assignment(std::move(lhs), std::move(rhs), std::move(lp)));
	return res;
}

std::unique_ptr<NullAssignment> tmr::SetNull (std::unique_ptr<Expr> lhs) {
	std::unique_ptr<NullAssignment> res(new NullAssignment(std::move(lhs)));
	return res;
}

std::unique_ptr<ReadInputAssignment> tmr::Read(std::string var) {
	std::unique_ptr<ReadInputAssignment> res(new ReadInputAssignment(Data(var)));
	return res;
}

std::unique_ptr<WriteOutputAssignment> tmr::Write(std::string var) {
	std::unique_ptr<WriteOutputAssignment> res(new WriteOutputAssignment(Data(var)));
	return res;
}


std::unique_ptr<LinearizationPoint> tmr::LinP(std::unique_ptr<Condition> cond) {
	std::unique_ptr<LinearizationPoint> res(new LinearizationPoint(std::move(cond)));
	return res;
}

std::unique_ptr<LinearizationPoint> tmr::LinP(std::string var) {
	std::unique_ptr<LinearizationPoint> res(new LinearizationPoint(Var(var)));
	return res;
}

std::unique_ptr<LinearizationPoint> tmr::LinP() {
	std::unique_ptr<LinearizationPoint> res(new LinearizationPoint());
	return res;
}

std::unique_ptr<Oracle> tmr::Orcl() {
	std::unique_ptr<Oracle> res(new Oracle());
	return res;
}

std::unique_ptr<CheckProphecy> tmr::ChkP(bool cond) {
	std::unique_ptr<CheckProphecy> res(new CheckProphecy(cond));
	return res;
}


std::unique_ptr<Ite> tmr::IfThen(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> ifs) {
	std::unique_ptr<Ite> res(new Ite(std::move(cond), std::move(ifs), Sqz()));
	return res;
}

std::unique_ptr<Ite> tmr::IfThenElse(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> ifs, std::unique_ptr<Sequence> elses) {
	std::unique_ptr<Ite> res(new Ite(std::move(cond), std::move(ifs), std::move(elses)));
	return res;
}

std::unique_ptr<While> tmr::Loop(std::unique_ptr<Sequence> body) {
	std::unique_ptr<While> res(new While(std::make_unique<TrueCondition>(), std::move(body)));
	return res;
}


std::unique_ptr<Free> tmr::Fr(std::string var) {
	std::unique_ptr<Free> res(new Free(Var(var)));
	return res;
}

std::unique_ptr<Malloc> tmr::Mllc(std::string var) {
	std::unique_ptr<Malloc> res(new Malloc(Var(var)));
	return res;
}

std::unique_ptr<Break> tmr::Brk() {
	std::unique_ptr<Break> res(new Break());
	return res;
}


std::unique_ptr<Killer> tmr::Kill(std::string var) {
	std::unique_ptr<Killer> res(new Killer(Var(var)));
	return res;
}
std::unique_ptr<Killer> tmr::Kill() {
	std::unique_ptr<Killer> res(new Killer());
	return res;
}


std::unique_ptr<CompareAndSwap> tmr::CAS(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, bool update_age_fields) {
	if (cmp->clazz() != Expr::VAR) throw std::logic_error("Second argument of CAS must be a VarExpr.");
	std::unique_ptr<CompareAndSwap> res(new CompareAndSwap(std::move(dst), std::move(cmp), std::move(src), update_age_fields));
	return res;
}

std::unique_ptr<CompareAndSwap> tmr::CAS(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, std::unique_ptr<LinearizationPoint> lp, bool update_age_fields) {
	if (cmp->clazz() != Expr::VAR) throw std::logic_error("Second argument of CAS must be a VarExpr.");
	std::unique_ptr<CompareAndSwap> res(new CompareAndSwap(std::move(dst), std::move(cmp), std::move(src), std::move(lp), update_age_fields));
	return res;
}


std::unique_ptr<Function> tmr::Fun(std::string name, bool is_void, std::unique_ptr<Sequence> body, std::unique_ptr<Atomic> summary) {
	std::unique_ptr<Function> res(new Function(name, is_void, std::move(body), std::move(summary)));
	return res;
}


/******************************** FUNCTION PROPAGATION ********************************/


void EqNeqCondition::propagateFun(const Function* fun) {}

void EqPtrAgeCondition::propagateFun(const Function* fun) {
	_cond->propagateFun(fun);
}

void CompoundCondition::propagateFun(const Function* fun) {
	_lhs->propagateFun(fun);
	_rhs->propagateFun(fun);
}

void CASCondition::propagateFun(const Function* fun) {
	_cas->propagateFun(fun);
}

void OracleCondition::propagateFun(const Function* fun) {}

void NonDetCondition::propagateFun(const Function* fun) {}

void TrueCondition::propagateFun(const Function* fun) {}

void Sequence::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	for (const auto& s : _stmts) s->propagateFun(fun);
}


void LinearizationPoint::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	assert(&event() == fun);
	assert(!fun->has_input() || !has_var());
	//assert(!is_prophet() || fun->has_output());
}

void Atomic::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	_sqz->propagateFun(fun);
}

void Assignment::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	if (_lp)
		_lp->propagateFun(fun);
}

void Ite::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	_cond->propagateFun(fun);
	_if->propagateFun(fun);
	_else->propagateFun(fun);
}

void While::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	_cond->propagateFun(fun);
	_stmts->propagateFun(fun);
}

void CompareAndSwap::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	if (_lp)
		_lp->propagateFun(fun);
}


/******************************** NAMECHECK ********************************/

void NullExpr::namecheck(const std::map<std::string, Variable*>& name2decl) {}

void VarExpr::namecheck(const std::map<std::string, Variable*>& name2decl) {
	assert(name2decl.count(_name));
	_decl = name2decl.at(_name);
}

void Selector::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_var->namecheck(name2decl);
	assert(_var->type() == POINTER);
}

void EqNeqCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_lhs->namecheck(name2decl);
	_rhs->namecheck(name2decl);
	assert(_lhs->type() == _rhs->type());
	assert(_lhs->type() == POINTER);
}

void CompoundCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_lhs->namecheck(name2decl);
	_rhs->namecheck(name2decl);
}

void EqPtrAgeCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_cond->namecheck(name2decl);
}

void CASCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_cas->namecheck(name2decl);
}

void OracleCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void NonDetCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void TrueCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void Sequence::namecheck(const std::map<std::string, Variable*>& name2decl) {
	for (const auto& stmt : _stmts) stmt->namecheck(name2decl);
}

void Atomic::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_sqz->namecheck(name2decl);
}

void Assignment::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_lhs->namecheck(name2decl);
	_rhs->namecheck(name2decl);
	assert(lhs().type() == rhs().type());
	assert(lhs().type() == POINTER);
	if (_lp) _lp->namecheck(name2decl);
}

void NullAssignment::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_lhs->namecheck(name2decl);
	assert(lhs().type() == POINTER);
}

void InOutAssignment::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_ptr->namecheck(name2decl);
	assert(_ptr->type() == DATA);
	assert((clazz() == INPUT && function().has_input()) || (clazz() == OUTPUT && function().has_output()));
}

void Malloc::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_var->namecheck(name2decl);
}

void Free::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_var->namecheck(name2decl);
}

void LinearizationPoint::namecheck(const std::map<std::string, Variable*>& name2decl) {
	if (_cond)
		_cond->namecheck(name2decl);
	if (_var)
		_var->namecheck(name2decl);
}

void Ite::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_cond->namecheck(name2decl);
	_if->namecheck(name2decl);
	_else->namecheck(name2decl);
}

void While::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_cond->namecheck(name2decl);
	_stmts->namecheck(name2decl);
}

void Break::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void CompareAndSwap::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_dst->namecheck(name2decl);
	_cmp->namecheck(name2decl);
	_src->namecheck(name2decl);
	if (_lp)
		_lp->namecheck(name2decl);
	// assert(_cmp->decl().local());
	// assert(_src->decl().local());
}

void Oracle::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void CheckProphecy::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void Killer::namecheck(const std::map<std::string, Variable*>& name2decl) {
	if (!_confused)
		_to_kill->namecheck(name2decl);
}

void Function::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_stmts->namecheck(name2decl);
}


/******************************** STATEMENT IDS ********************************/

std::size_t CASCondition::propagateId(std::size_t id) {
	return _cas->propagateId(id);
}

std::size_t Sequence::propagateId(std::size_t id) {
	id = Statement::propagateId(id);
	for (const auto& s : _stmts) id = s->propagateId(id);
	return id;
}

std::size_t Atomic::propagateId(std::size_t id) {
	id = Statement::propagateId(id);
	id = _sqz->propagateId(id);
	return id;
}

std::size_t Ite::propagateId(std::size_t id) {
	id = Statement::propagateId(id);
	id = _cond->propagateId(id);
	id = _if->propagateId(id);
	id = _else->propagateId(id);
	return id;
}

std::size_t While::propagateId(std::size_t id) {
	id = Statement::propagateId(id);
	id = _cond->propagateId(id);
	id = _stmts->propagateId(id);
	return id;
}

std::size_t Assignment::propagateId(std::size_t id) {
	id = Statement::propagateId(id);
	if (_lp)
		id = _lp->propagateId(id);
	return id;
}

std::size_t CompareAndSwap::propagateId(std::size_t id) {
	id = Statement::propagateId(id);
	if (_lp)
		id = _lp->propagateId(id);
	return id;
}


/******************************** NEXT PROGRAM COUNTER ********************************/

const Statement* Sequence::next() const {
	return _next;
}

void Sequence::propagateNext(const Statement* next, const While* last_while) {
	if (size() == 0) _next = next;
	else {
		_next = _stmts.front().get();
		for (std::size_t i = 0; i + 1 < _stmts.size(); i++) _stmts[i]->propagateNext(_stmts[i+1].get(), last_while);
		_stmts.back()->propagateNext(next, last_while);
	}
}

void Atomic::propagateNext(const Statement* next, const While* last_while) {
	_next = next;
	_sqz->propagateNext(NULL, NULL /* never break out of an atomic block */);
}

void Assignment::propagateNext(const Statement* next, const While* last_while) {
	if (_lp) {
		Statement::propagateNext(_lp.get(), last_while);
		_lp->propagateNext(next, last_while);
	} else {
		Statement::propagateNext(next, last_while);	
	}
	// Statement::propagateNext(next, last_while);
	// if (_lp)
	//	_lp->propagateNext(_next, last_while);
}

const Statement* Ite::next() const {
	if (_cond->type() == Condition::CASC) return NULL;

	assert(false);
	throw std::logic_error("Malicious call to Ite::next()");
}

const Statement* Ite::next_true_branch() const {
	return _if->next();
}

const Statement* Ite::next_false_branch() const {
	return _else->next();
}

void Ite::propagateNext(const Statement* next, const While* last_while) {
	_next = NULL;
	_if->propagateNext(next, last_while);
	_else->propagateNext(next, last_while);
}

const Statement* While::next() const {
	assert(false);
	throw std::logic_error("Malicious call to While::next()");
}

const Statement* While::next_true_branch() const {
	return _stmts->size() == 0 ? _next : _stmts->next();
}

const Statement* While::next_false_branch() const {
	return _next;
}

void While::propagateNext(const Statement* next, const While* last_while) {
	assert(_stmts->size() > 0);
	_next = next;
	_stmts->propagateNext(this, this);
}

void Break::propagateNext(const Statement* next, const While* last_while) {
	assert(last_while != NULL);
	_next = last_while->next_false_branch();
}


/******************************** PRINTING ********************************/

#if PRINT_ID
	#define printID os << "[" << id() << "] ";
	#define printID_(id) os << "[" << id << "] ";
#else
	#define printID ;
	#define printID_(id) ;
#endif

inline void printCAS(std::ostream& os, const CompareAndSwap& cas) {
	printID_(cas.id());
	os << "CAS(" << cas.dst() << ", " << cas.cmp() << ", ";
	if (cas.update_age_fields()) os << "<" << cas.src() << ", " << cas.cmp() << ".age+1>";
	else os << cas.src();
	if (cas.fires_lp()) os << " " << cas.lp();
	os << ")";
}

#define INDENT(i) for(std::size_t j = 0; j < i; j++) os<<"    ";

std::ostream& tmr::operator<<(std::ostream& os, const Variable& var) {
	os << var.name();
	return os;
}

std::ostream& tmr::operator<<(std::ostream& os, const Expr& expr) {
	expr.print(os);
	return os;
}

void NullExpr::print(std::ostream& os) const {
	os << "NULL";
}

void VarExpr::print(std::ostream& os) const {
	os << _name;
}

void Selector::print(std::ostream& os) const {
	os << _var->name() << (_selection == POINTER ? ".next" : ".data");
}

std::ostream& tmr::operator<<(std::ostream& os, const Condition& stmt) {
	stmt.print(os);
	return os;
}

void EqNeqCondition::print(std::ostream& os) const {
	_lhs->print(os);
	if (is_inverted()) os << " != ";
	else os << " == ";
	_rhs->print(os);
}

void CompoundCondition::print(std::ostream& os) const {
	_lhs->print(os);
	os << " && ";
	_rhs->print(os);
}

void EqPtrAgeCondition::print(std::ostream& os) const {
	os << "<" << _cond->lhs() << ", " << _cond->lhs() << ".age> == <" << _cond->rhs() << ", " << _cond->rhs() << ".age>";
}

void CASCondition::print(std::ostream& os) const {
	printCAS(os, *_cas);
}

void OracleCondition::print(std::ostream& os) const {
	os << "§prophecy == fulfilled";
}

void NonDetCondition::print(std::ostream& os) const {
	os << "*";
}

void TrueCondition::print(std::ostream& os) const {
	os << "true";
}

std::ostream& tmr::operator<<(std::ostream& os, const Statement& stmt) {
	stmt.print(os, 0);
	return os;
}

void Sequence::print(std::ostream& os, std::size_t indent) const {
	os << "{ ";
	if (indent != 0) os << std::endl;
	for (const auto& s : _stmts)
		if (indent == 0) {
			s->print(os, 0);
			os << " ";
		}
		else {
			INDENT(indent+1);
			s->print(os, indent + 1);
			os << std::endl;
		}
	INDENT(indent);
	os << "} ";
	// if (indent > 0) os << std::endl;
}

void Atomic::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "Atomic ";
	_sqz->print(os, indent == 0 ? 0 : indent);
}

void Assignment::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << lhs() << " = " << rhs();
	if (fires_lp()) os << " " << *_lp;
	os << ";";
}

void NullAssignment::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << lhs() << " = NULL;";
}

void ReadInputAssignment::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << expr() << " = " << "__in__" << ";";
}

void WriteOutputAssignment::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "__out__ = " << expr() << ";";
}


void Malloc::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "malloc(" << var() << ");";
}

void Free::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "free(" << var() << ");";
}

void LinearizationPoint::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "*** ";
	if (has_cond()) os << "[" << cond() << "] ";
	if (event().has_input()) {
		os << event().name() << "(__in__) ";
	} else {
		os << event().name() << "(";
		if (has_var()) os << _var->name() << ".data";
		else os << "empty";
		os << ") ";
	}
	os << "***";
}

void Ite::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "if (";
	_cond->print(os);
	os << ") ";
	_if->print(os, indent == 0 ? 0 : indent);
	if (_else && _else->size() > 0) {
		os << " else ";
		_else->print(os, indent == 0 ? 0 : indent);
	}
}

void While::print(std::ostream& os, std::size_t indent) const {
	os << "while (";
	_cond->print(os);
	os << ") ";
	_stmts->print(os, indent);
}

void Break::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "break;";
}

void CompareAndSwap::print(std::ostream& os, std::size_t indent) const {
	printCAS(os, *this);
	os << ";";
}

void Oracle::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "oracle(§prophecy);";
}

void CheckProphecy::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "assume(§prophecy == " << (_cond ? "fulfilled" : "wrong") << ");";
}

void Killer::print(std::ostream& os, std::size_t indent) const {
	printID;
	if (_confused) os << "kill_confused();";
	else os << "kill(" << var() << ");";
}

std::ostream& tmr::operator<<(std::ostream& os, const Function& fun) {
	fun.print(os);
	return os;
}

void Function::print(std::ostream& os, std::size_t indent) const {
	os << std::endl << std::endl;
	INDENT(indent);
	os << "function[";
	if (has_input()) os << "?";
	else os << "!";
	os << "] " << _name << "(";
	if (has_input()) os << input_name();
	else os << output_name();
	os << ") ";
	_stmts->print(os, indent);
}

std::ostream& tmr::operator<<(std::ostream& os, const Program& prog) {
	prog.print(os);
	return os;
}

void Program::print(std::ostream& os) const {
	os << "PROGRAM " << _name << " BEGIN" << std::endl;
	INDENT(1);
	os << "GLOBALS: ";
	if (_globals.size() > 0) os << _globals.front()->name();
	for (std::size_t i = 1; i < _globals.size(); i++) os << ", " << _globals.at(i)->name();
	os << ";" << std::endl;
	INDENT(1);
	os << "LOCALS: ";
	if (_locals.size() > 0) os << _locals.front()->name();
	for (std::size_t i = 1; i < _locals.size(); i++) os << ", " << _locals.at(i)->name();
	os << ";" << std::endl;
	os << std::endl;
	if (_init()) {
		INDENT(1);
		os << "init";
		_init()->print(os, 1);
	}
	for (const auto& f : _funs) f->print(os, 1);
	#if REPLACE_INTERFERENCE_WITH_SUMMARY
		for (const auto& f : _funs) {
			os << std::endl << std::endl;
			INDENT(1);
			os << "summary(" << f->name() << "): ";
			f->_summary->print(os, 1);
		}
	#endif
	os << std::endl << "END" << std::endl;
}
