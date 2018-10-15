#include "prog.hpp"

#include <stdexcept>
#include <set>
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

Program::Program(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::unique_ptr<Sequence> init, std::unique_ptr<Sequence> init_thread, std::vector<std::unique_ptr<Function>> funs)
	: _name(name),
	  _globals(mk_vars(true, globals)),
	  _locals(mk_vars(false, locals)),
	  _funs(std::move(funs)),
	  _init_fun(new Function("_init", std::move(init))),
	  _init_thread_fun(new Function("_threadinit", std::move(init_thread))) {

	if (uses_reserved_name(_funs)) {
		throw std::logic_error("Function names must not start with '_'.");
	}

	// namecheck, typecheck
	std::map<std::string, Variable*> name2decl;
	for (const auto& v : _globals) {
		assert(name2decl.count(v->name()) == 0);
		name2decl[v->name()] = v.get();
	}

	if (name2decl.count("__in__") > 0) throw std::logic_error("Variable name '__in__' is reserved.");
	if (name2decl.count("__out__") > 0) throw std::logic_error("Variable name '__out__' is reserved.");

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
	for (const auto& f : _funs)
		id = f->propagateId(id);
	_idSize = id;

	// ensure that init does only consist of simple statements
	const Statement* fi = _init();
	while (fi != NULL) {
		assert(!fi->is_conditional());
		fi = fi->next();
	}
}


Function::Function(std::string name, std::unique_ptr<Sequence> stmts) : _name(name), _stmts(std::move(stmts)) {
		_stmts->propagateFun(this);
}

Assignment::Assignment(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) : _lhs(std::move(lhs)), _rhs(std::move(rhs)), _fires_lp(false) {
	assert(_lhs->clazz() != Expr::NIL);
	assert(_rhs->clazz() != Expr::NIL);
}

CompareAndSwap::CompareAndSwap(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, bool update_age_fields)
	: _dst(std::move(dst)), _cmp(std::move(cmp)), _src(std::move(src)), _update_age_fields(update_age_fields) {
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


std::unique_ptr<Assignment> tmr::Assign (std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs) {
	if (rhs->clazz() == Expr::NIL) throw std::logic_error("Assigning NULL not supported. Use NullAssignment/SetNull instead.");
	std::unique_ptr<Assignment> res(new Assignment(std::move(lhs), std::move(rhs)));
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


std::unique_ptr<Killer> tmr::Kill(std::string var) {
	std::unique_ptr<Killer> res(new Killer(Var(var)));
	return res;
}


std::unique_ptr<CompareAndSwap> tmr::CAS(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, bool update_age_fields) {
	if (cmp->clazz() != Expr::VAR) throw std::logic_error("Second argument of CAS must be a VarExpr.");
	std::unique_ptr<CompareAndSwap> res(new CompareAndSwap(std::move(dst), std::move(cmp), std::move(src), update_age_fields));
	return res;
}


std::unique_ptr<Function> tmr::Fun(std::string name, std::unique_ptr<Sequence> body) {
	std::unique_ptr<Function> res(new Function(name, std::move(body)));
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

void NonDetCondition::propagateFun(const Function* fun) {}

void TrueCondition::propagateFun(const Function* fun) {}

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

void EqPtrAgeCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_cond->namecheck(name2decl);
}

void CASCondition::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_cas->namecheck(name2decl);
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
}

void NullAssignment::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_lhs->namecheck(name2decl);
	assert(lhs().type() == POINTER);
}

void InOutAssignment::namecheck(const std::map<std::string, Variable*>& name2decl) {
	_ptr->namecheck(name2decl);
	assert(_ptr->type() == DATA);

	if (expr().clazz() == Expr::VAR) {
		if (!static_cast<const VarExpr&>(expr()).decl().local()) {
			throw std::logic_error("Input/Output statements must target local variables.");
		}
	}
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
	os << "CAS(" << cas.dst() << ", " << cas.cmp() << ", ";
	if (cas.update_age_fields()) os << "<" << cas.src() << ", " << cas.cmp() << ".age+1>";
	else os << cas.src();
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

std::ostream& tmr::operator<<(std::ostream& os, const Function& fun) {
	fun.print(os);
	return os;
}

void Function::print(std::ostream& os, std::size_t indent) const {
	os << std::endl << std::endl;
	INDENT(indent);
	os << "function " << _name << "(" << arg_name() << ") ";
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
	os << std::endl << "END" << std::endl;
}
