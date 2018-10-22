#include "prog.hpp"

#include <stdexcept>
#include <set>
#include "config.hpp"

using namespace tmr;


/******************************** CONSTRUCTION ********************************/

static std::vector<std::unique_ptr<Variable>> mk_vars(bool global, std::vector<std::string> names) {
	std::vector<std::unique_ptr<Variable>> result(names.size());
	for (std::size_t i = 0; i < result.size(); i++) {
		assert(names[i] != "__arg__");
		result[i] = std::make_unique<Variable>(names[i], i, global);
	}
	return result;
}

bool startsWith(std::string mainStr, std::string toMatch) {
	// taken from: https://thispointer.com/c-check-if-a-string-starts-with-an-another-given-string/
	if(mainStr.find(toMatch) == 0) return true;
	else return false;
}

static bool uses_reserved_name(const std::vector<std::unique_ptr<Function>>& funs) {
	for (const auto& f : funs) {
		auto fname = f->name();
		if (startsWith(fname, "_"))
			return true;
	}
	return false;
}

Program::Program(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::unique_ptr<Sequence> init,
	   std::unique_ptr<Sequence> init_thread, std::vector<std::unique_ptr<Function>> funs)
	: _name(name),
	  _globals(mk_vars(true, globals)),
	  _locals(mk_vars(false, locals)),
	  _funs(std::move(funs)),
	  _init_fun(new Function("__init__", std::move(init), false)),
	  _init_thread_fun(new Function("__thread_init__", std::move(init_thread), false)) {

	if (uses_reserved_name(_funs)) {
		throw std::logic_error("Function names must not start with '_'.");
	}

	// namecheck, typecheck
	std::map<std::string, Variable*> name2decl;
	for (const auto& v : _globals) {
		assert(name2decl.count(v->name()) == 0);
		name2decl[v->name()] = v.get();
	}

	if (name2decl.count("__arg__") > 0) throw std::logic_error("Variable name '__arg__' is reserved.");
	if (name2decl.count("__rec__") > 0) throw std::logic_error("Variable name '__rec__' is reserved.");
	if (name2decl.count("NULL") > 0) throw std::logic_error("Variable name 'NULL' is reserved.");

	// init must only access global variables
	_init_fun->namecheck(name2decl);

	for (const auto& v : _locals) {
		assert(name2decl.count(v->name()) == 0);
		name2decl[v->name()] = v.get();
	}

	_init_thread_fun->namecheck(name2decl);
	for (const auto& f : _funs) {
		f->namecheck(name2decl);
	}
	for (const auto& f : _funs) f->_prog = this;
	_init_fun->_prog = this;
	_init_thread_fun->_prog = this;

	// set variable ids
	for (std::size_t i = 0; i < _globals.size(); i++) _globals[i]->_id = i;
	for (std::size_t i = 0; i < _locals.size(); i++) _locals[i]->_id = i;

	// propagate stmt next field
	for (const auto& f : _funs) f->propagateNext();
	_init_fun->propagateNext();
	_init_thread_fun->propagateNext();

	// propagate statement ids
	std::size_t id = 1; // id=0 is reserved for NULL
	id = _init_fun->propagateId(id);
	id = _init_thread_fun->propagateId(id);
	for (const auto& f : _funs)
		id = f->propagateId(id);
	_idSize = id;

	// ensure that init does only consist of simple statements
	const Statement* fi = _init_fun->_stmts.get();
	while (fi != NULL) {
		if (fi->is_conditional()) {
			throw std::logic_error("Initialization function init() must not contain conditionals.");
		}
		fi = fi->next();
	}

	_init_fun->checkRecInit();
	_init_thread_fun->checkRecInit();
	for (const auto& f : _funs)
		f->checkRecInit();
}


Function::Function(std::string name, std::unique_ptr<Sequence> stmts, bool has_arg) : _name(name), _stmts(std::move(stmts)), _has_arg(has_arg) {
	_stmts->propagateFun(this);
}

Assignment::Assignment(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) : _lhs(std::move(lhs)), _rhs(std::move(rhs)) {
	assert(_lhs->clazz() != Expr::NIL);
	assert(_rhs->clazz() != Expr::NIL);
}

CompareAndSwap::CompareAndSwap(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src)
	: _dst(std::move(dst)), _cmp(std::move(cmp)), _src(std::move(src)) {
	assert(_dst->clazz() == Expr::VAR || _dst->clazz() == Expr::SEL);
	assert(_cmp->clazz() == Expr::VAR || _cmp->clazz() == Expr::NIL);
	assert(_src->clazz() == Expr::VAR || _src->clazz() == Expr::SEL);
}

CompoundCondition::CompoundCondition(std::unique_ptr<Condition> lhs, std::unique_ptr<Condition> rhs) : _lhs(std::move(lhs)), _rhs(std::move(rhs)) {
	assert(_lhs->type() != COMPOUND);
}


/******************************** SHORTCUTS ********************************/

std::unique_ptr<VarExpr> tmr::Var (std::string name) {
	std::unique_ptr<VarExpr> res(new VarExpr(name));
	return res;
}

std::unique_ptr<Selector> tmr::Next(std::string name) {
	std::unique_ptr<Selector> res(new Selector(Var(name), POINTER));
	return res;
}

std::unique_ptr<Selector> tmr::Data(std::string name, std::size_t index) {
	std::unique_ptr<Selector> res(new Selector(Var(name), DATA, index));
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

std::unique_ptr<CASCondition> tmr::CasCond(std::unique_ptr<CompareAndSwap> cas) {
	std::unique_ptr<CASCondition> res(new CASCondition(std::move(cas)));
	return res;
}

std::unique_ptr<NonDetCondition> tmr::NDCond() {
	std::unique_ptr<NonDetCondition> res(new NonDetCondition());
	return res;
}

std::unique_ptr<CompoundCondition> tmr::CompCond(std::unique_ptr<Condition> lhs, std::unique_ptr<Condition> rhs) {
	assert(lhs->type() != Condition::CASC);
	assert(rhs->type() != Condition::CASC);
	std::unique_ptr<CompoundCondition> res(new CompoundCondition(std::move(lhs), std::move(rhs)));
	return res;
}

std::unique_ptr<EpochVarCondition> tmr::EpochCond() {
	std::unique_ptr<EpochVarCondition> res(new EpochVarCondition());
	return res;
}

std::unique_ptr<EpochSelCondition> tmr::EpochCond(std::string name) {
	std::unique_ptr<EpochSelCondition> res(new EpochSelCondition(Var(name)));
	return res;
}



std::unique_ptr<Assignment> tmr::Assign (std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
	if (rhs->clazz() == Expr::NIL) throw std::logic_error("Assigning NULL not supported. Use NullAssignment/SetNull instead.");
	std::unique_ptr<Assignment> res(new Assignment(std::move(lhs), std::move(rhs)));
	return res;
}

std::unique_ptr<NullAssignment> tmr::SetNull (std::unique_ptr<Expr> lhs) {
	std::unique_ptr<NullAssignment> res(new NullAssignment(std::move(lhs)));
	return res;
}

std::unique_ptr<WriteRecData> tmr::WriteRecArg(std::size_t index) {
	std::unique_ptr<WriteRecData> res(new WriteRecData(index, WriteRecData::FROM_ARG));
	return res;
}

std::unique_ptr<WriteRecData> tmr::WriteRecNull(std::size_t index) {
	std::unique_ptr<WriteRecData> res(new WriteRecData(index, WriteRecData::FROM_NULL));
	return res;
}

std::unique_ptr<SetRecEpoch> tmr::SetEpoch() {
	std::unique_ptr<SetRecEpoch> res(new SetRecEpoch());
	return res;
}

std::unique_ptr<GetLocalEpochFromGlobalEpoch> tmr::GetEpoch() {
	std::unique_ptr<GetLocalEpochFromGlobalEpoch> res(new GetLocalEpochFromGlobalEpoch());
	return res;
}

std::unique_ptr<InitRecPtr> tmr::InitRec(std::string name) {
	std::unique_ptr<InitRecPtr> res(new InitRecPtr(Var(name)));
	return res;
}

std::unique_ptr<IncrementGlobalEpoch> tmr::Inc() {
	std::unique_ptr<IncrementGlobalEpoch> res(new IncrementGlobalEpoch());
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

std::unique_ptr<Malloc> tmr::Mllc(std::string var) {
	std::unique_ptr<Malloc> res(new Malloc(Var(var)));
	return res;
}

std::unique_ptr<Break> tmr::Brk() {
	std::unique_ptr<Break> res(new Break());
	return res;
}

std::unique_ptr<FreeAll> tmr::Free(std::size_t setid) {
	std::unique_ptr<FreeAll> res(new FreeAll(setid));
	return res;
}


std::unique_ptr<Killer> tmr::Kill(std::string var) {
	std::unique_ptr<Killer> res(new Killer(Var(var)));
	return res;
}


std::unique_ptr<CompareAndSwap> tmr::CAS(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src) {
	if (cmp->clazz() != Expr::VAR) throw std::logic_error("Second argument of CAS must be a VarExpr.");
	std::unique_ptr<CompareAndSwap> res(new CompareAndSwap(std::move(dst), std::move(cmp), std::move(src)));
	return res;
}


std::unique_ptr<SetAddArg> tmr::AddArg(std::size_t lhs) {
	return std::make_unique<SetAddArg>(lhs);
}

std::unique_ptr<SetAddSel> tmr::AddSel(std::size_t lhs, std::unique_ptr<Selector> sel) {
	return std::make_unique<SetAddSel>(lhs, std::move(sel));
}

std::unique_ptr<SetCombine> tmr::Combine(std::size_t lhs, std::size_t rhs, SetCombine::Type type) {
	return std::make_unique<SetCombine>(lhs, rhs, type);
}

std::unique_ptr<SetCombine> tmr::SetAssign(std::size_t lhs, std::size_t rhs) {
	return Combine(lhs, rhs, SetCombine::SETTO);
}

std::unique_ptr<SetCombine> tmr::SetMinus(std::size_t lhs, std::size_t rhs) {
	return Combine(lhs, rhs, SetCombine::SUBTRACTION);
}

std::unique_ptr<SetClear> tmr::Clear(std::size_t lhs) {
	return std::make_unique<SetClear>(lhs);
}


std::unique_ptr<Function> tmr::Fun(std::string name, std::unique_ptr<Sequence> body, bool has_arg) {
	std::unique_ptr<Function> res(new Function(name, std::move(body), has_arg));
	return res;
}


/******************************** FUNCTION PROPAGATION ********************************/


void EqNeqCondition::propagateFun(const Function* fun) {}

void CompoundCondition::propagateFun(const Function* fun) {
	_lhs->propagateFun(fun);
	_rhs->propagateFun(fun);
}

void CASCondition::propagateFun(const Function* fun) {
	_cas->propagateFun(fun);
}

void NonDetCondition::propagateFun(const Function* fun) {}

void TrueCondition::propagateFun(const Function* fun) {}

void EpochVarCondition::propagateFun(const Function* fun) {}

void EpochSelCondition::propagateFun(const Function* fun) {}

void Sequence::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	for (const auto& s : _stmts) s->propagateFun(fun);
}


void Atomic::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
	_sqz->propagateFun(fun);
}

void Assignment::propagateFun(const Function* fun) {
	Statement::propagateFun(fun);
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
}


/******************************** NAMECHECK ********************************/

void NullExpr::namecheck(const std::map<std::string, Variable*>& name2decl) {}

void VarExpr::namecheck(const std::map<std::string, Variable*>& name2decl) {
	if (!name2decl.count(_name)) {
		throw std::logic_error("Undeclared variable: " + _name);
	}
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

void CASCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_cas->namecheck(name2decl);
}

void NonDetCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void TrueCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void EpochVarCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void EpochSelCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_cmp->namecheck(name2decl);
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
}

void NullAssignment::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_lhs->namecheck(name2decl);
	assert(_lhs->clazz() != Expr::VAR || _lhs->type() == POINTER);
}

void WriteRecData::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void SetRecEpoch::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void GetLocalEpochFromGlobalEpoch::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void InitRecPtr::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_rhs->namecheck(name2decl);
	if (rhs().type() != POINTER) {
		throw std::logic_error("__rec__ must be initialized from pointer variables");
	}
}

void IncrementGlobalEpoch::namecheck(const std::map<std::string, Variable*>& name2decl) {
}

void Malloc::namecheck(const std::map<std::string, Variable*>& name2decl) {
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
}

void Killer::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_to_kill->namecheck(name2decl);
}

void SetAddArg::namecheck(const std::map<std::string, Variable*>& name2decl) {
	if (!function().has_arg()) {
		throw std::logic_error("Function argument used in function without argument.");
	}
}

void SetAddSel::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_sel->namecheck(name2decl);
}

void Function::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_stmts->namecheck(name2decl);
}


/******************************** CHECK INIT REC ********************************/

std::ostream& tmr::operator<<(std::ostream& os, const std::set<const Variable*>& set) {
	os << "{ ";
	for (const Variable* v : set) {
		if (v) os << *v << ", ";
		else os << "NULL, ";
	}
	os << " }";
	return os;
}

void Sequence::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	for (const auto& stmt : _stmts) {
		stmt->checkRecInit(fromAllocation);
	}
}

void Atomic::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	_sqz->checkRecInit(fromAllocation);
}

void Assignment::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	const Expr& expr = *_lhs;
	if (expr.clazz() == Expr::VAR) {
		const VarExpr& lhs = static_cast<const VarExpr&>(expr);
		fromAllocation.erase(&lhs.decl());
	}
}

void NullAssignment::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void WriteRecData::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void SetRecEpoch::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void GetLocalEpochFromGlobalEpoch::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void InitRecPtr::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	if (fromAllocation.count(&_rhs->decl()) == 0) {
		throw std::logic_error("__rec__ must be initialized from the last allocation.");
	}
}

void IncrementGlobalEpoch::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void Malloc::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	fromAllocation.insert(&_var->decl());
}

void Break::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void Conditional::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	// TODO: check for cas cond
	const Condition& cond = *_cond;
	if (cond.type() == Condition::CASC) {
		const CASCondition& cascond = static_cast<const CASCondition&>(cond);
		cascond.cas().checkRecInit(fromAllocation);
	}
}

void Ite::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	Conditional::checkRecInit(fromAllocation);
	
	std::set<const Variable*> sIf(fromAllocation);
	std::set<const Variable*> sElse(fromAllocation);

	_if->checkRecInit(sIf);
	_if->checkRecInit(sElse);

	fromAllocation.clear();
	std::set_intersection(sIf.begin(),sIf.end(),sElse.begin(),sElse.end(), std::inserter(fromAllocation,fromAllocation.begin()));
}

void While::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	Conditional::checkRecInit(fromAllocation);

	std::set<const Variable*> sWhile(fromAllocation);
	std::set<const Variable*> sCopy(fromAllocation);

	_stmts->checkRecInit(sWhile);

	fromAllocation.clear();
	std::set_intersection(sWhile.begin(),sWhile.end(),sCopy.begin(),sCopy.end(), std::inserter(fromAllocation,fromAllocation.begin()));
}

void CompareAndSwap::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	const Expr& expr = *_dst;
	if (expr.clazz() == Expr::VAR) {
		const VarExpr& dst = static_cast<const VarExpr&>(expr);
		fromAllocation.erase(&dst.decl());
	}
}

void Killer::checkRecInit(std::set<const Variable*>& fromAllocation) const {
	fromAllocation.erase(&_to_kill->decl());
}

void SetOperation::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void SetAddArg::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void SetAddSel::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void SetCombine::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void SetClear::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void FreeAll::checkRecInit(std::set<const Variable*>& fromAllocation) const {
}

void Function::checkRecInit() const {
	std::set<const Variable*> fromAllocation;
	_stmts->checkRecInit(fromAllocation);
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
	return id;
}

std::size_t CompareAndSwap::propagateId(std::size_t id) {
	id = Statement::propagateId(id);
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
	Statement::propagateNext(next, last_while);	
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
	os << "CAS(" << cas.dst() << ", " << cas.cmp() << ", " << cas.src() << ")";
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
	os << _var->name();
	if  (_selection == POINTER) {
		os << "->next";
	} else {
		os << "->data" << _index;
	}
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

void CASCondition::print(std::ostream& os) const {
	printCAS(os, *_cas);
}

void NonDetCondition::print(std::ostream& os) const {
	os << "*";
}

void TrueCondition::print(std::ostream& os) const {
	os << "true";
}

void EpochVarCondition::print(std::ostream& os) const {
	os << "epoch != Epoch";
}

void EpochSelCondition::print(std::ostream& os) const {
	os << "epoch != " << *_cmp << "->epoch";
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
	os << ";";
}

void NullAssignment::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << lhs() << " = NULL;";
}

void WriteRecData::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "__rec__->data" << _sel_index << " = ";
	switch (_type) {
		case FROM_ARG: os << "__arg__" << ";"; break;
		case FROM_NULL: os << "NULL" << ";"; break;
	}
}

void SetRecEpoch::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "__rec__->epoch = epoch;";
}

void GetLocalEpochFromGlobalEpoch::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "epoch = Epoch;";
}

void InitRecPtr::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "__rec__ = " << rhs() << ":";
}

void IncrementGlobalEpoch::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "Epoch = (Epoch + 1) mod 3;";
}


void Malloc::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "malloc(" << var() << ");";
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

void Killer::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "kill(" << var() << ");";
}

#define printSet(x) os << "set" << x;

void SetAddArg::print(std::ostream& os, std::size_t indent) const {
	printID;
	printSet(setid());
	os << ".add(__arg__);";
}

void SetAddSel::print(std::ostream& os, std::size_t indent) const {
	printID;
	printSet(setid());
	os << ".add(";
	_sel->print(os);
	os << ");";
}

void SetCombine::print(std::ostream& os, std::size_t indent) const {
	printID;
	printSet(lhs());
	switch (_type) {
		case Type::SETTO: os << " = "; printSet(rhs()); break;
		case Type::UNION: os << ".add_all("; printSet(rhs()); os << ")"; break;
		case Type::SUBTRACTION: os << ".remove_all("; printSet(rhs()); os << ")"; break;
	}
	os << ";";
}

void SetClear::print(std::ostream& os, std::size_t indent) const {
	printID;
	printSet(setid());
	os << " = ∅;";
}

void FreeAll::print(std::ostream& os, std::size_t indent) const {
	printID;
	os << "free_all(";
	printSet(setid());
	std::cout << ");";
}

std::ostream& tmr::operator<<(std::ostream& os, const Function& fun) {
	fun.print(os);
	return os;
}

void Function::print(std::ostream& os, std::size_t indent) const {
	os << std::endl;
	INDENT(indent);
	os << "function " << _name << "(";
	if (_has_arg) {
		os << "data_t " << arg_name();
	}
	os << ") ";
	_stmts->print(os, indent);
	os << std::endl;
}

std::ostream& tmr::operator<<(std::ostream& os, const Program& prog) {
	prog.print(os);
	return os;
}

inline void printNodeStruct(std::ostream& os) {
	INDENT(1);
	os << "struct Node {" << std::endl;
	INDENT(2);
	os << "ptr_t next;" << std::endl;
	INDENT(2);
	os << "data_t data0;" << std::endl;
	INDENT(2);
	os << "data_t data1;" << std::endl;
	INDENT(2);
	os << "time_t epoch;" << std::endl;
	INDENT(1);
	os << "}" << std::endl << std::endl;
}

inline void printFreeAllMacro(std::ostream& os) {
	INDENT(1);
	os << "macro free_all(Set<data_t> set) {" << std::endl;
	INDENT(2);
	os << "for (data_t x : set) {" << std::endl;
	INDENT(3);
	os << "if (x != NULL) {" << std::endl;
	INDENT(4);
	os << "free(x);" << std::endl;
	INDENT(3);
	os << "}" << std::endl;
	INDENT(2);
	os << "}" << std::endl;
	INDENT(1);
	os << "}" << std::endl;
}

void Program::print(std::ostream& os) const {
	os << "PROGRAM " << _name << " BEGIN" << std::endl;
	// global ptr variables
	INDENT(1);
	os << "GLOBALS: " << std::endl;
	INDENT(2);
	os << "ptr_t ";
	if (_globals.size() > 0) os << _globals.front()->name();
	for (std::size_t i = 1; i < _globals.size(); i++) os << ", " << _globals.at(i)->name();
	os << ";" << std::endl;
	INDENT(2);
	os << "time_t Epoch;" << std::endl;
	// local ptr variables
	std::cout << std::endl;
	INDENT(1);
	os << "LOCALS: " << std::endl;
	INDENT(2);
	os << "ptr_t __rec__;" << std::endl;
	INDENT(2);
	os << "ptr_t ";
	if (_locals.size() > 0) os << _locals.front()->name();
	for (std::size_t i = 1; i < _locals.size(); i++) os << ", " << _locals.at(i)->name();
	os << ";" << std::endl;
	// local predefined variables
	INDENT(2);
	os << "time_t epoch;" << std::endl;
	INDENT(2);
	os << "Set<data_t> set0, set1, set2;" << std::endl;
	// free_all macro
	os << std::endl;
	printNodeStruct(os);
	printFreeAllMacro(os);
	// functions
	os << std::endl << std::endl;
	INDENT(1);
	os << "/* initialization */" << std::endl;
	_init_fun->print(os, 1);
	_init_thread_fun->print(os, 1);
	os << std::endl << std::endl;
	INDENT(1);
	os << "/* API */" << std::endl;
	for (const auto& f : _funs) f->print(os, 1);
	os << "END" << std::endl;
}
