#pragma once

#include <vector>
#include <set>
#include <memory>
#include <string>
#include <iostream>
#include <map>
#include <functional>
#include <assert.h>


namespace tmr {

	enum Type { POINTER, DATA };

	class Variable;

	class Expr;
	class NullExpr;
	class VarExpr;
	class Selector;

	class Condition;
	class EqNeqCondition;
	class CASCondition;
	class TrueCondition;

	class Statement;
	class Sequence;
	class Atomic;
	class Assignment;
	class NullAssignment;
	class Malloc;
	class Conditional;
	class Ite;
	class While;
	class Break;
	class CompareAndSwap;
	class Function;
	class Program;

	class Observer;

	class Variable {
		private:
			std::string _name;
			unsigned short _id;
			bool _is_global;

		public: 
			Variable(std::string name, unsigned short id, bool global) : _name(name), _id(id), _is_global(global) {}
			std::string name() const { return _name; }
			unsigned short id() const { return _id; }
			Type type() const { return POINTER; }
			bool global() const { return _is_global; }
			bool local() const { return !_is_global; }

		friend class Program;
	};

	/*********************** EXPR ***********************/

	class Expr {
		public:
			enum Class { NIL=0, VAR=1, SEL=2 };
			virtual ~Expr() = default;
			virtual Type type() const = 0;
			virtual Expr::Class clazz() const = 0;
			virtual void print(std::ostream& os) const = 0;
			virtual void namecheck(const std::map<std::string, Variable*>& name2decl) = 0;
	};

	class NullExpr : public Expr {
		public:
			Type type() const { return POINTER; }
			Expr::Class clazz() const { return Expr::Class::NIL; }
			void print(std::ostream& os) const;
			void namecheck(const std::map<std::string, Variable*>& name2decl);
	};

	class VarExpr : public Expr {
		private:
			const Variable* _decl;
			std::string _name;

		public:
			Type type() const { assert(_decl != NULL); return _decl->type(); }
			Expr::Class clazz() const { return Expr::Class::VAR; }
			void print(std::ostream& os) const;
			void namecheck(const std::map<std::string, Variable*>& name2decl);

			VarExpr(std::string name) : _name(name) {}
			std::string name() const { return _name; }
			const Variable& decl() const { assert(_decl != NULL); return *_decl; }
	};

	class Selector : public Expr {
		private:
			std::unique_ptr<VarExpr> _var;
			Type _selection;
			std::size_t _index;

		public:
			Type type() const { return _selection; }
			Expr::Class clazz() const { return Expr::Class::SEL; }
			void print(std::ostream& os) const;
			void namecheck(const std::map<std::string, Variable*>& name2decl);

			Selector(std::unique_ptr<VarExpr> var, Type sel) : _var(std::move(var)), _selection(sel), _index(0) {}
			Selector(std::unique_ptr<VarExpr> var, Type sel, std::size_t index) : _var(std::move(var)), _selection(sel), _index(index) { assert(sel == DATA); assert(index < 2); }
			const Variable& decl() const { return _var->decl(); }
			std::size_t index() const { return _index; }
	};


	/*********************** CONDITION ***********************/

	class Condition {
		public:
			virtual ~Condition() = default;
			enum Type { EQNEQ, CASC, TRUEC, COMPOUND, ORACLEC, NONDET, EPOCH_VAR, EPOCH_SEL };
			virtual Type type() const = 0;
			virtual void namecheck(const std::map<std::string, Variable*>& name2decl) = 0;
			virtual void print(std::ostream& os) const = 0;
			virtual void propagateFun(const Function* fun) = 0;
			virtual std::size_t propagateId(std::size_t id) { return id; }
	};

	class EqNeqCondition : public Condition {
		private:
			std::unique_ptr<Expr> _lhs;
			std::unique_ptr<Expr> _rhs;
			bool _inverted;

		public:
			EqNeqCondition(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs, bool neq=false) : _lhs(std::move(lhs)), _rhs(std::move(rhs)), _inverted(neq) {}
			const Expr& lhs() const { return *_lhs; }
			const Expr& rhs() const { return *_rhs; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			bool is_inverted() const { return _inverted; }
			Type type() const { return Type::EQNEQ; }
			void propagateFun(const Function* fun);
	};

	class CompoundCondition : public Condition {
		private:
			std::unique_ptr<Condition> _lhs;
			std::unique_ptr<Condition> _rhs;

		public:
			CompoundCondition(std::unique_ptr<Condition> lhs, std::unique_ptr<Condition> rhs);
			const Condition& lhs() const { return *_lhs; }
			const Condition& rhs() const { return *_rhs; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			Type type() const { return Type::COMPOUND; }
			void propagateFun(const Function* fun);
	};

	class NonDetCondition : public Condition {
		public:
			Type type() const { return Type::NONDET; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			void propagateFun(const Function* fun);
	};

	class CASCondition : public Condition {
		private:
			std::unique_ptr<CompareAndSwap> _cas;

		public:
			CASCondition(std::unique_ptr<CompareAndSwap> cas) : _cas(std::move(cas)) { assert(_cas.get() != NULL); }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			Type type() const { return Type::CASC; }
			const CompareAndSwap& cas() const { return *_cas; }
			void propagateFun(const Function* fun);
			std::size_t propagateId(std::size_t id);
	};

	class TrueCondition : public Condition {
		public: 
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			Type type() const { return Type::TRUEC; }
			void propagateFun(const Function* fun);
	};

	class EpochVarCondition : public Condition {
		public: 
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			Type type() const { return Type::EPOCH_VAR; }
			void propagateFun(const Function* fun);
	};

	class EpochSelCondition : public Condition {
		private:
			std::unique_ptr<VarExpr> _cmp;
		public: 
			EpochSelCondition(std::unique_ptr<VarExpr> cmp) : _cmp(std::move(cmp)) {}
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			Type type() const { return Type::EPOCH_SEL; }
			void propagateFun(const Function* fun);
			const VarExpr& var() const { return *_cmp; }
	};


	/*********************** STATEMENT ***********************/

	class Statement {
		private:
			unsigned short _id;
			const Function* _parent = NULL;

		protected:
			const Statement* _next = NULL;

		public:
			enum Class {
				SQZ, ASSIGN, MALLOC, ITE, WHILE, BREAK, WRITEREC, CAS, SETNULL, ATOMIC, KILL, SETADD_ARG,
				SETADD_SEL, SETCOMBINE, SETCLEAR, FREEALL, INITREC, SETEPOCH, GETEPOCH, INC
			};
			virtual ~Statement() = default;
			virtual Class clazz() const = 0;
			unsigned short id() const { assert(_id != 0); return _id; }
			const Function& function() const { assert(_parent != NULL); return *_parent; }
			virtual bool is_conditional() const { return false; }
			virtual const Statement* next() const { return _next; }
			virtual void propagateFun(const Function* fun) { assert(fun != NULL); _parent = fun; }
			virtual void propagateNext(const Statement* next, const While* last_while) { _next = next; }
			virtual std::size_t propagateId(std::size_t id) { _id = id; return id+1; }
			virtual void namecheck(const std::map<std::string, Variable*>& name2decl) = 0;
			virtual void print(std::ostream& os, std::size_t indent) const = 0;
			virtual void checkRecInit(std::set<const Variable*>& fromAllocation) const = 0;
	};

	class Sequence : public Statement {
		private:
			std::vector<std::unique_ptr<Statement>> _stmts;

		public:
			Class clazz() const { return Statement::Class::SQZ; }
			const Statement* next() const;
			void propagateNext(const Statement* next, const While* last_while);
			std::size_t propagateId(std::size_t id);
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void propagateFun(const Function* fun);
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;

			Sequence(std::vector<std::unique_ptr<Statement>> stmts) : _stmts(std::move(stmts)) {}
			std::size_t size() const { return _stmts.size(); }
			const Statement& at(std::size_t index) const { return *(_stmts.at(index)); }
	};

	class Atomic : public Statement {
		private:
			std::unique_ptr<Sequence> _sqz;

		public:
			Class clazz() const { return Statement::ATOMIC; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void propagateFun(const Function* fun);
			void propagateNext(const Statement* next, const While* last_while);
			std::size_t propagateId(std::size_t id);
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;

			Atomic(std::unique_ptr<Sequence> sqz) : _sqz(std::move(sqz)) {
				// assert(_sqz->size() > 0);
			}
			const Sequence& sqz() const { return *_sqz; }
	};

	class Assignment : public Statement {
		private:
			std::unique_ptr<Expr> _lhs;
			std::unique_ptr<Expr> _rhs;

		public:
			Statement::Class clazz() const { return Statement::Class::ASSIGN; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			std::size_t propagateId(std::size_t id);
			void propagateNext(const Statement* next, const While* last_while);
			void propagateFun(const Function* fun);
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;

			Assignment(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);
			const Expr& lhs() const { return *_lhs; }
			const Expr& rhs() const { return *_rhs; }
	};

	class NullAssignment : public Statement {
		private:
			std::unique_ptr<Expr> _lhs;

		public:
			Statement::Class clazz() const { return Statement::Class::SETNULL; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;

			NullAssignment(std::unique_ptr<Expr> lhs) : _lhs(std::move(lhs)) {
				assert(_lhs->clazz() == Expr::VAR || _lhs->clazz() == Expr::SEL);
			}
			const Expr& lhs() const { return *_lhs; }
	};

	class WriteRecData : public Statement {
		public:
			enum Type { FROM_ARG, FROM_NULL };

		private:
			std::size_t _sel_index;
			Type _type;

		public:
			WriteRecData(std::size_t index, Type type) : _sel_index(index), _type(type) { assert(_sel_index < 2); }
			Statement::Class clazz() const { return Statement::Class::WRITEREC; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			std::size_t index() const { return _sel_index; }
			Type type() const { return _type; }
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class SetRecEpoch : public Statement {
		public:
			Statement::Class clazz() const { return Statement::Class::SETEPOCH; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class GetLocalEpochFromGlobalEpoch : public Statement {
		public:
			Statement::Class clazz() const { return Statement::Class::GETEPOCH; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class InitRecPtr : public Statement {
		private:
			std::unique_ptr<VarExpr> _rhs;

		public:
			InitRecPtr(std::unique_ptr<VarExpr> rhs) : _rhs(std::move(rhs)) {}
			Statement::Class clazz() const { return Statement::Class::INITREC; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			const VarExpr& rhs() const { return *_rhs; }
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class IncrementGlobalEpoch : public Statement {
		public:
			Statement::Class clazz() const { return Statement::Class::INC; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class Malloc : public Statement {
		private:
			std::unique_ptr<VarExpr> _var;

		public:
			Statement::Class clazz() const { return Statement::Class::MALLOC; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;

			Malloc(std::unique_ptr<VarExpr> var) : _var(std::move(var)) {}
			const VarExpr& var() const { return *_var; }
			const Variable& decl() const { return _var->decl(); }
	};

	class Break : public Statement {
		public:
			Statement::Class clazz() const { return Statement::Class::BREAK; }
			void propagateNext(const Statement* next, const While* last_while);
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class Conditional : public Statement {
		protected:
			std::unique_ptr<Condition> _cond;
		public:
			Conditional(std::unique_ptr<Condition> cond) : _cond(std::move(cond)) {}
			virtual ~Conditional() = default;
			bool is_conditional() const { return true; }
			const Condition& cond() const { return *_cond; }
			virtual const Statement* next_true_branch() const = 0;
			virtual const Statement* next_false_branch() const = 0;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class Ite : public Conditional {
		private:
			std::unique_ptr<Sequence> _if;
			std::unique_ptr<Sequence> _else;

		public:
			Statement::Class clazz() const { return Statement::Class::ITE; }
			const Statement* next() const;
			void propagateNext(const Statement* next, const While* last_while);
			std::size_t propagateId(std::size_t id);
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void propagateFun(const Function* fun);
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;

			Ite(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> ifstmts, std::unique_ptr<Sequence> elsestmts)
			   : Conditional(std::move(cond)), _if(std::move(ifstmts)), _else(std::move(elsestmts)) {}
			const Statement* next_true_branch() const;
			const Statement* next_false_branch() const;
	};

	class While : public Conditional {
		private:
			std::unique_ptr<Sequence> _stmts;

		public:
			Statement::Class clazz() const { return Statement::Class::WHILE; }
			const Statement* next() const;
			void propagateNext(const Statement* next, const While* last_while);
			std::size_t propagateId(std::size_t id);
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void propagateFun(const Function* fun);
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;

			While(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> stmts) : Conditional(std::move(cond)), _stmts(std::move(stmts)) {}
			const Statement* next_true_branch() const;
			const Statement* next_false_branch() const;
	};

	class CompareAndSwap : public Statement {
		private:
			std::unique_ptr<Expr> _dst;
			std::unique_ptr<Expr> _cmp;
			std::unique_ptr<Expr> _src;

		public:
			Statement::Class clazz() const { return Statement::Class::CAS; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			std::size_t propagateId(std::size_t id);
			void propagateFun(const Function* fun);
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;

			CompareAndSwap(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src);
			const Expr& dst() const { return *_dst; }
			const Expr& cmp() const { return *_cmp; }
			const Expr& src() const { return *_src; }
	};

	class Killer : public Statement {
		private:
			std::unique_ptr<VarExpr> _to_kill;

		public:
			Statement::Class clazz() const { return Statement::Class::KILL; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			Killer(std::unique_ptr<VarExpr> to_kill) : _to_kill(std::move(to_kill)) {}
			const VarExpr& var() const { return *_to_kill; }
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class SetOperation : public Statement {
		private:
			std::size_t _setid;

		public:
			SetOperation(std::size_t setid) : _setid(setid) { assert(_setid <= 2); }
			virtual ~SetOperation() = default;
			virtual Statement::Class clazz() const = 0;
			virtual void namecheck(const std::map<std::string, Variable*>& name2decl) {}
			virtual void print(std::ostream& os, std::size_t indent) const = 0;
			std::size_t setid() const { return _setid; }
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class SetAddArg : public SetOperation {
		public:
			SetAddArg(std::size_t setid) : SetOperation(setid) {}
			Statement::Class clazz() const { return Statement::SETADD_ARG; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class SetAddSel : public SetOperation {
		private:
			std::unique_ptr<Selector> _sel;

		public:
			SetAddSel(std::size_t setid, std::unique_ptr<Selector> sel) : SetOperation(setid), _sel(std::move(sel)) { assert(_sel->type() == DATA); }
			Statement::Class clazz() const { return Statement::SETADD_SEL; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			const Selector& selector() const { return *_sel; }
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class SetCombine : public SetOperation {
		public:
			enum Type { UNION, SUBTRACTION, SETTO };

		private:
			std::size_t _rhs;
			Type _type;

		public:
			SetCombine(std::size_t lhs, std::size_t rhs, SetCombine::Type type) : SetOperation(lhs), _rhs(rhs), _type(type) { assert(rhs <= 2); }
			Statement::Class clazz() const { return Statement::SETCOMBINE; }
			void print(std::ostream& os, std::size_t indent) const;
			std::size_t lhs() const { return setid(); }
			std::size_t rhs() const { return _rhs; }
			SetCombine::Type type() const { return _type; }
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class SetClear : public SetOperation {
		public:
			SetClear(std::size_t setid) : SetOperation(setid) {}
			Statement::Class clazz() const { return Statement::SETCLEAR; }
			void print(std::ostream& os, std::size_t indent) const;
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	class FreeAll : public Statement {
		private:
			std::size_t _setid;

		public:
			FreeAll(std::size_t setid) : _setid(setid) { assert(setid <= 2); }
			Statement::Class clazz() const { return Statement::Class::FREEALL; }
			void namecheck(const std::map<std::string, Variable*>& name2decl) {}
			void print(std::ostream& os, std::size_t indent) const;
			std::size_t setid() const { return _setid; }
			void checkRecInit(std::set<const Variable*>& fromAllocation) const;
	};

	/*********************** PROGRAM ***********************/

	class Function {
		private:
			std::string _name;
			std::unique_ptr<Sequence> _stmts;
			const Program* _prog;
			bool _has_arg;

		public:
			Function(std::string name, std::unique_ptr<Sequence> stmts, bool has_arg);
			std::string name() const { return _name; }
			std::size_t size() const { return _stmts->size(); }
			const Sequence& body() const { return *_stmts; }
			const Statement& at(std::size_t index) const { return _stmts->at(index); }
			void propagateNext() { _stmts->propagateNext(NULL, NULL); }
			std::size_t propagateId(std::size_t id) { return _stmts->propagateId(id); }
			void print(std::ostream& os, std::size_t indent=0) const;
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			std::string arg_name() const { return "__arg__"; }
			bool has_arg() const { return _has_arg; }
			void checkRecInit() const;
			const Program& prog() const { return *_prog; }

		friend class Program;
	};

	class Program {
		private:
			std::string _name;
			std::vector<std::unique_ptr<Variable>> _globals;
			std::vector<std::unique_ptr<Variable>> _locals;
			std::vector<std::unique_ptr<Function>> _funs;
			std::unique_ptr<Function> _init_fun;
			std::unique_ptr<Function> _init_thread_fun;
			std::size_t _idSize = 0;

		public:
			Program(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::unique_ptr<Sequence> init, std::unique_ptr<Sequence> init_thread, std::vector<std::unique_ptr<Function>> funs);
			std::string name() const { return _name; }
			std::size_t size() const { return _funs.size(); }
			std::size_t idSize() const { return _idSize; }
			std::size_t numGlobals() const { return _globals.size(); }
			std::size_t numLocals() const { return _locals.size(); }
			const Function& at(std::size_t index) const { return *_funs.at(index); }
			const Sequence& init() const { return _init_fun->body(); }
			const Sequence& init_thread() const { return _init_thread_fun->body(); }
			void print(std::ostream& os) const;
	};


	/*********************** PRINTING ***********************/

	std::ostream& operator<<(std::ostream& os, const Variable& var);
	std::ostream& operator<<(std::ostream& os, const Expr& expr);
	std::ostream& operator<<(std::ostream& os, const Condition& stmt);
	std::ostream& operator<<(std::ostream& os, const Statement& stmt);
	std::ostream& operator<<(std::ostream& os, const Function& fun);
	std::ostream& operator<<(std::ostream& os, const Program& prog);
	std::ostream& operator<<(std::ostream& os, const std::set<const Variable*>& set);


	/*********************** SHORTCUTS ***********************/

	std::unique_ptr<VarExpr>  Var (std::string name);
	std::unique_ptr<Selector> Next(std::string name);
	std::unique_ptr<Selector> Data(std::string name, std::size_t index);
	std::unique_ptr<NullExpr> Null();

	std::unique_ptr<EqNeqCondition> EqCond(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);
	std::unique_ptr<EqNeqCondition> NeqCond(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);
	std::unique_ptr<CASCondition> CasCond(std::unique_ptr<CompareAndSwap> cas);
	std::unique_ptr<CompoundCondition> CompCond(std::unique_ptr<Condition> lhs, std::unique_ptr<Condition> rhs);
	std::unique_ptr<NonDetCondition> NDCond();
	std::unique_ptr<EpochVarCondition> EpochCond();
	std::unique_ptr<EpochSelCondition> EpochCond(std::string name);

	std::unique_ptr<Assignment> Assign (std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);
	std::unique_ptr<NullAssignment> SetNull (std::unique_ptr<Expr> lhs);
	std::unique_ptr<WriteRecData> WriteRecArg(std::size_t index);
	std::unique_ptr<WriteRecData> WriteRecNull(std::size_t index);
	std::unique_ptr<SetRecEpoch> SetEpoch();
	std::unique_ptr<GetLocalEpochFromGlobalEpoch> GetEpoch();
	std::unique_ptr<InitRecPtr> InitRec(std::string name);
	std::unique_ptr<IncrementGlobalEpoch> Inc();

	std::unique_ptr<Ite> IfThen(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> ifs);
	std::unique_ptr<Ite> IfThenElse(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> ifs, std::unique_ptr<Sequence> elses);
	std::unique_ptr<While> Loop(std::unique_ptr<Sequence> body);

	std::unique_ptr<Malloc> Mllc(std::string var);
	std::unique_ptr<Break> Brk();
	std::unique_ptr<Killer> Kill(std::string var);
	std::unique_ptr<FreeAll> Free(std::size_t setid);

	std::unique_ptr<SetAddArg> AddArg(std::size_t lhs);
	std::unique_ptr<SetAddSel> AddSel(std::size_t lhs, std::unique_ptr<Selector> sel);
	std::unique_ptr<SetCombine> Combine(std::size_t lhs, std::size_t rhs, SetCombine::Type comb);
	std::unique_ptr<SetCombine> SetAssign(std::size_t lhs, std::size_t rhs);
	std::unique_ptr<SetCombine> SetMinus(std::size_t lhs, std::size_t rhs);
	std::unique_ptr<SetClear> Clear(std::size_t lhs);

	std::unique_ptr<CompareAndSwap> CAS(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src);

	std::unique_ptr<Function> Fun(std::string name, std::unique_ptr<Sequence> body, bool has_arg);

	template<typename... Args>
	static std::unique_ptr<Sequence> Sqz(Args... pack) {
		// std::vector<std::unique_ptr<Statement>> stmts { std::move(pack)... }; // does not work :(
		std::unique_ptr<Statement> args[] { std::move(pack)... };
		std::vector<std::unique_ptr<Statement>> stmts;
		for (auto& s : args) stmts.push_back(std::move(s));
		std::unique_ptr<Sequence> res(new Sequence(std::move(stmts)));
		return res;
	}

	template<typename... Args>
	static std::unique_ptr<Atomic> AtomicSqz(Args... pack) {
		std::unique_ptr<Atomic> res(new Atomic(Sqz(std::move(pack)...)));
		return res;
	}

	template<typename... Args>
	static std::unique_ptr<Program> Prog(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::unique_ptr<Sequence> init, std::unique_ptr<Sequence> init_thread, Args... funpack) {
		// std::vector<std::unique_ptr<Function>> funs { std::move(funpack)... }; // does not work :(
		std::unique_ptr<Function> args[] { std::move(funpack)... };
		std::vector<std::unique_ptr<Function>> funs;
		for (auto& f : args) funs.push_back(std::move(f));
		std::unique_ptr<Program> res(new Program(name, std::move(globals), std::move(locals), std::move(init), std::move(init_thread), std::move(funs)));
		return res;
	}

}

#include "observer.hpp"
