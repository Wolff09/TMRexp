#pragma once

#include <vector>
#include <memory>
#include <string>
#include <iostream>
#include <map>
#include <functional>
#include <assert.h>
#include "make_unique.hpp"


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
	class InOutAssignment;
	class ReadInputAssignment;
	class WriteOutputAssignment;
	class Malloc;
	// class Free;
	class Retire;
	class HPset;
	class Conditional;
	class Ite;
	class While;
	class Break;
	class LinearizationPoint;
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

		public:
			Type type() const { return _selection; }
			Expr::Class clazz() const { return Expr::Class::SEL; }
			void print(std::ostream& os) const;
			void namecheck(const std::map<std::string, Variable*>& name2decl);

			Selector(std::unique_ptr<VarExpr> var, Type sel) : _var(std::move(var)), _selection(sel) {}
			const Variable& decl() const { return _var->decl(); }
	};


	/*********************** CONDITION ***********************/

	class Condition {
		public:
			enum Type { EQNEQ, CASC, TRUEC, WAGE, COMPOUND, ORACLEC, NONDET };
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

	class OracleCondition : public Condition {
		public:
			Type type() const { return Type::ORACLEC; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			void propagateFun(const Function* fun);
	};

	class NonDetCondition : public Condition {
		public:
			Type type() const { return Type::NONDET; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os) const;
			void propagateFun(const Function* fun);
	};

	class EqPtrAgeCondition : public Condition {
		private:
			std::unique_ptr<EqNeqCondition> _cond;

		public:
			EqPtrAgeCondition(std::unique_ptr<VarExpr> lhs, std::unique_ptr<VarExpr> rhs) : _cond(new EqNeqCondition(std::move(lhs), std::move(rhs))) {}
			virtual Type type() const { return Type::WAGE; }
			virtual void namecheck(const std::map<std::string, Variable*>& name2decl);
			virtual void print(std::ostream& os) const;
			virtual void propagateFun(const Function* fun);
			const EqNeqCondition& cond() const { return *_cond; }
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


	/*********************** STATEMENT ***********************/

	class Statement {
		private:
			unsigned short _id;
			const Function* _parent = NULL;

		protected:
			const Statement* _next = NULL;

		public:
			enum Class { SQZ, ASSIGN, MALLOC, /*FREE,*/ RETIRE, HPSET, HPRELEASE, ITE, WHILE, BREAK, LINP, INPUT, OUTPUT, CAS, SETNULL, ATOMIC, ORACLE, CHECKP, KILL, REACH };
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

			Atomic(std::unique_ptr<Sequence> sqz) : _sqz(std::move(sqz)) {
				assert(_sqz->size() > 0);
			}
			const Sequence& sqz() const { return *_sqz; }
	};

	class LinearizationPoint : public Statement {
		private:
			std::unique_ptr<VarExpr> _var;
			std::unique_ptr<Condition> _cond;

		public:
			Statement::Class clazz() const { return Statement::Class::LINP; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			void propagateFun(const Function* fun);

			LinearizationPoint();
			LinearizationPoint(std::unique_ptr<Condition> cond);
			LinearizationPoint(std::unique_ptr<VarExpr> var);
			LinearizationPoint(std::unique_ptr<VarExpr> var, std::unique_ptr<Condition> cond);
			bool has_var() const { return _var.get() != NULL; }
			bool has_cond() const { return _cond.get() != NULL; }
			const Condition& cond() const { assert(has_cond()); return *_cond; }
			const VarExpr& var() const { assert(has_var()); return *_var; }
			const Function& event() const { return function(); }
	};

	class Assignment : public Statement {
		private:
			std::unique_ptr<Expr> _lhs;
			std::unique_ptr<Expr> _rhs;
			std::unique_ptr<LinearizationPoint> _lp;
			bool _fires_lp;

		public:
			Statement::Class clazz() const { return Statement::Class::ASSIGN; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			std::size_t propagateId(std::size_t id);
			void propagateNext(const Statement* next, const While* last_while);
			void propagateFun(const Function* fun);

			Assignment(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);
			Assignment(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs, std::unique_ptr<LinearizationPoint> lp);
			const Expr& lhs() const { return *_lhs; }
			const Expr& rhs() const { return *_rhs; }
			bool fires_lp() const { return _fires_lp; }
			const LinearizationPoint& lp() const { assert(fires_lp()); return *_lp; }
	};

	class NullAssignment : public Statement {
		private:
			std::unique_ptr<Expr> _lhs;

		public:
			Statement::Class clazz() const { return Statement::Class::SETNULL; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;

			NullAssignment(std::unique_ptr<Expr> lhs) : _lhs(std::move(lhs)) {
				assert(_lhs->clazz() == Expr::VAR || _lhs->clazz() == Expr::SEL);
			}
			const Expr& lhs() const { return *_lhs; }
	};

	class InOutAssignment : public Statement {
		private:
			std::unique_ptr<Expr> _ptr;

		public:
			InOutAssignment(std::unique_ptr<Expr> expr) : _ptr(std::move(expr)) {}
			virtual Statement::Class clazz() const = 0;
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			virtual void print(std::ostream& os, std::size_t indent) const = 0;
			const Expr& expr() const { assert(_ptr.get() != NULL); return *_ptr; }
	};

	class ReadInputAssignment : public InOutAssignment {
		public:
			ReadInputAssignment(std::unique_ptr<Expr> dst) : InOutAssignment(std::move(dst)) {}
			Statement::Class clazz() const { return Statement::INPUT; }
			void print(std::ostream& os, std::size_t indent) const;
	};

	class WriteOutputAssignment : public InOutAssignment {
		public:
			WriteOutputAssignment(std::unique_ptr<Expr> src) : InOutAssignment(std::move(src)) {}
			Statement::Class clazz() const { return Statement::OUTPUT; }
			void print(std::ostream& os, std::size_t indent) const;
	};

	class Malloc : public Statement {
		private:
			std::unique_ptr<VarExpr> _var;

		public:
			Statement::Class clazz() const { return Statement::Class::MALLOC; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;

			Malloc(std::unique_ptr<VarExpr> var) : _var(std::move(var)) {}
			const VarExpr& var() const { return *_var; }
			const Variable& decl() const { return _var->decl(); }
	};

	// class Free : public Statement {
	// 	private:
	// 		std::unique_ptr<VarExpr> _var;

	// 	public:
	// 		Statement::Class clazz() const { return Statement::Class::FREE; }
	// 		void namecheck(const std::map<std::string, Variable*>& name2decl);
	// 		void print(std::ostream& os, std::size_t indent) const;

	// 		Free(std::unique_ptr<VarExpr> var) : _var(std::move(var)) {}
	// 		const VarExpr& var() const { return *_var; }
	// 		const Variable& decl() const { return _var->decl(); }
	// };

	class Retire : public Statement {
		private:
			std::unique_ptr<VarExpr> _var;

		public:
			Statement::Class clazz() const { return Statement::Class::RETIRE; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;

			Retire(std::unique_ptr<VarExpr> var) : _var(std::move(var)) {}
			const VarExpr& var() const { return *_var; }
			const Variable& decl() const { return _var->decl(); }
	};

	class HPset : public Statement {
		private:
			std::unique_ptr<VarExpr> _var;
			std::size_t _hpindex;

		public:
			Statement::Class clazz() const { return Statement::Class::HPSET; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;

			HPset(std::unique_ptr<VarExpr> var, std::size_t index) : _var(std::move(var)), _hpindex(index) {}
			const VarExpr& var() const { return *_var; }
			const Variable& decl() const { return _var->decl(); }
			std::size_t hpindex() const { return _hpindex; }
	};

	class HPrelease : public Statement {
		private:
			std::size_t _hpindex;

		public:
			Statement::Class clazz() const { return Statement::Class::HPRELEASE; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;

			HPrelease(std::size_t index) : _hpindex(index) {}
			std::size_t hpindex() const { return _hpindex; }
	};

	class Break : public Statement {
		public:
			Statement::Class clazz() const { return Statement::Class::BREAK; }
			void propagateNext(const Statement* next, const While* last_while);
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
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

			While(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> stmts) : Conditional(std::move(cond)), _stmts(std::move(stmts)) {}
			const Statement* next_true_branch() const;
			const Statement* next_false_branch() const;
	};

	/**
	 * @brief CAS(a, b, c) is an atomic statement which (1) checks whether a==b hold, and only if so,
	 *        (2) sets a=c and (3) a.age = b.age+1
	 */
	class CompareAndSwap : public Statement {
		private:
			std::unique_ptr<Expr> _dst;
			std::unique_ptr<Expr> _cmp;
			std::unique_ptr<Expr> _src;
			std::unique_ptr<LinearizationPoint> _lp;
			bool _update_age_fields;

		public:
			Statement::Class clazz() const { return Statement::Class::CAS; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			std::size_t propagateId(std::size_t id);
			void propagateFun(const Function* fun);

			CompareAndSwap(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, bool update_age_fields);
			CompareAndSwap(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, std::unique_ptr<LinearizationPoint> lp, bool update_age_fields);
			const Expr& dst() const { return *_dst; }
			const Expr& cmp() const { return *_cmp; }
			const Expr& src() const { return *_src; }
			bool fires_lp() const { return _lp.get() != NULL; }
			const LinearizationPoint& lp() const { assert(fires_lp()); return *_lp; }
			bool update_age_fields() const { return _update_age_fields; }
	};

	class Oracle : public Statement {
		public:
			Statement::Class clazz() const { return Statement::Class::ORACLE; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
	};

	class CheckProphecy : public Statement {
		private:
			bool _cond;

		public:
			Statement::Class clazz() const { return Statement::Class::CHECKP; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			CheckProphecy(bool cond) : _cond(cond) {}
			bool cond() const { return _cond; }
	};

	class Killer : public Statement {
		private:
			bool _confused;
			std::unique_ptr<VarExpr> _to_kill;

		public:
			Statement::Class clazz() const { return Statement::Class::KILL; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			Killer() : _confused(true) {}
			Killer(std::unique_ptr<VarExpr> to_kill) : _confused(false), _to_kill(std::move(to_kill)) {}
			const VarExpr& var() const { return *_to_kill; }
			bool kill_confused() const { return _confused; }
	};

	class EnforceReach : public Statement {
		private:
			std::unique_ptr<VarExpr> _to_check;

		public:
			Statement::Class clazz() const { return Statement::Class::REACH; }
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			void print(std::ostream& os, std::size_t indent) const;
			EnforceReach(std::unique_ptr<VarExpr> to_check) : _to_check(std::move(to_check)) {}
			const VarExpr& var() const { return *_to_check; }
	};

	/*********************** PROGRAM ***********************/

	class Function {
		private:
			std::string _name;
			std::unique_ptr<Sequence> _stmts;
			bool _has_input; // a function has either input or output
			const Program* _prog;
			std::unique_ptr<Atomic> _summary;

		public:
			Function(std::string name, bool is_void, std::unique_ptr<Sequence> stmts);
			Function(std::string name, bool is_void, std::unique_ptr<Sequence> stmts, std::unique_ptr<Atomic> summary);
			std::string name() const { return _name; }
			std::size_t size() const { return _stmts->size(); }
			const Sequence& body() const { return *_stmts; }
			const Statement& at(std::size_t index) const { return _stmts->at(index); }
			void propagateNext() { _stmts->propagateNext(NULL, NULL); }
			std::size_t propagateId(std::size_t id) { return _stmts->propagateId(id); }
			void print(std::ostream& os, std::size_t indent=0) const;
			void namecheck(const std::map<std::string, Variable*>& name2decl);
			bool has_input() const { return _has_input; }
			bool has_output() const { return !has_input(); }		
			std::string input_name() const { assert(has_input()); return "__in__"; }
			std::string output_name() const { assert(has_output()); return "__out__"; }
			const Program& prog() const { return *_prog; }
			const Atomic& summary() const { assert(_summary); return *_summary; }

		friend class Program;
	};

	class Program {
		private:
			std::string _name;
			std::vector<std::unique_ptr<Variable>> _globals;
			std::vector<std::unique_ptr<Variable>> _locals;
			std::vector<std::unique_ptr<Function>> _funs;
			std::unique_ptr<Function> _free, _guard, _unguard, _retire;
			std::unique_ptr<Function> _init_fun;
			std::size_t _idSize = 0;
			Sequence* _init() const { return _init_fun->_stmts.get(); }
			std::unique_ptr<Observer> _smrobs;
			bool _has_hint = false;
			std::function<bool(void*)> _hint;
			bool _increase_precision_chk_mimick = false;

		public:
			Program(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::vector<std::unique_ptr<Function>> funs);
			Program(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::unique_ptr<Sequence> init, std::vector<std::unique_ptr<Function>> funs);
			std::string name() const { return _name; }
			std::size_t size() const { return _funs.size(); }
			std::size_t idSize() const { return _idSize; }
			std::size_t numGlobals() const { return _globals.size(); }
			std::size_t numLocals() const { return _locals.size(); }
			const Function& at(std::size_t index) const { return *_funs.at(index); }
			const Function& freefun() const { return *_free; }
			const Function& guardfun() const { return *_guard; }
			const Function& unguardfun() const { return *_unguard; }
			const Function& retirefun() const { return *_retire; }
			const Sequence& init() const { return _init_fun->body(); }
			const Function& init_fun() const { return *_init_fun; }
			void print(std::ostream& os) const;
			bool is_summary_statement(const Statement& stmt) const;
			void set_hint(std::function<bool(void*)> hint) { _has_hint = true; _hint = hint; }
			bool has_hint() const { return _has_hint; }
			bool apply_hint(void* param) const { assert(has_hint()); return _hint(param); }
			void set_chk_mimic_precision(bool val) { _increase_precision_chk_mimick = val; }
			bool precise_check_mimick() const { return _increase_precision_chk_mimick; }
			const Observer& smr_observer() const { return *_smrobs; }
			void smr_observer(std::unique_ptr<Observer> smr) { _smrobs = std::move(smr); }
	};


	/*********************** PRINTING ***********************/

	std::ostream& operator<<(std::ostream& os, const Variable& var);
	std::ostream& operator<<(std::ostream& os, const Expr& expr);
	std::ostream& operator<<(std::ostream& os, const Condition& stmt);
	std::ostream& operator<<(std::ostream& os, const Statement& stmt);
	std::ostream& operator<<(std::ostream& os, const Function& fun);
	std::ostream& operator<<(std::ostream& os, const Program& prog);


	/*********************** SHORTCUTS ***********************/

	std::unique_ptr<VarExpr>  Var (std::string name);
	std::unique_ptr<Selector> Next(std::string name);
	std::unique_ptr<Selector> Data(std::string name);
	std::unique_ptr<NullExpr> Null();

	std::unique_ptr<EqNeqCondition> EqCond(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);
	std::unique_ptr<EqNeqCondition> NeqCond(std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);
	std::unique_ptr<EqPtrAgeCondition> EqCondWAge(std::unique_ptr<VarExpr> lhs, std::unique_ptr<VarExpr> rhs);
	std::unique_ptr<CASCondition> CasCond(std::unique_ptr<CompareAndSwap> cas);
	std::unique_ptr<Condition> EqCond(std::unique_ptr<VarExpr> lhs, std::unique_ptr<VarExpr> rhs, bool use_age_fields);
	std::unique_ptr<CompoundCondition> CompCond(std::unique_ptr<Condition> lhs, std::unique_ptr<Condition> rhs);
	std::unique_ptr<OracleCondition> OCond();
	std::unique_ptr<NonDetCondition> NDCond();

	std::unique_ptr<Assignment> Assign (std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs);
	std::unique_ptr<Assignment> Assign (std::unique_ptr<Expr> lhs, std::unique_ptr<Expr> rhs, std::unique_ptr<LinearizationPoint> lp);
	std::unique_ptr<NullAssignment> SetNull (std::unique_ptr<Expr> lhs);
	std::unique_ptr<ReadInputAssignment> Read(std::string var);
	std::unique_ptr<WriteOutputAssignment> Write(std::string var);

	std::unique_ptr<LinearizationPoint> LinP(std::unique_ptr<Condition> cond);
	std::unique_ptr<LinearizationPoint> LinP(std::string var);
	std::unique_ptr<LinearizationPoint> LinP();
	std::unique_ptr<Oracle> Orcl();
	std::unique_ptr<CheckProphecy> ChkP(bool cond);

	std::unique_ptr<Ite> IfThen(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> ifs);
	std::unique_ptr<Ite> IfThenElse(std::unique_ptr<Condition> cond, std::unique_ptr<Sequence> ifs, std::unique_ptr<Sequence> elses);
	std::unique_ptr<While> Loop(std::unique_ptr<Sequence> body);

	std::unique_ptr<Malloc> Mllc(std::string var);
	// std::unique_ptr<Free> Fr(std::string var);
	std::unique_ptr<Retire> Rtire(std::string var);
	std::unique_ptr<HPset> Gard(std::string var, std::size_t index);
	std::unique_ptr<HPrelease> UGard(std::size_t index);
	std::unique_ptr<Break> Brk();
	std::unique_ptr<Killer> Kill(std::string var);
	std::unique_ptr<Killer> Kill();
	std::unique_ptr<EnforceReach> ChkReach(std::string var);

	std::unique_ptr<CompareAndSwap> CAS(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, bool update_age_fields);
	std::unique_ptr<CompareAndSwap> CAS(std::unique_ptr<Expr> dst, std::unique_ptr<Expr> cmp, std::unique_ptr<Expr> src, std::unique_ptr<LinearizationPoint> lp, bool update_age_fields);

	std::unique_ptr<Function> Fun(std::string name, bool is_void, std::unique_ptr<Sequence> body, std::unique_ptr<Atomic> summary = {});

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
	static std::unique_ptr<Program> Prog(std::string name, std::vector<std::string> globals, std::vector<std::string> locals, std::unique_ptr<Sequence> init, Args... funpack) {
		// std::vector<std::unique_ptr<Function>> funs { std::move(funpack)... }; // does not work :(
		std::unique_ptr<Function> args[] { std::move(funpack)... };
		std::vector<std::unique_ptr<Function>> funs;
		for (auto& f : args) funs.push_back(std::move(f));
		std::unique_ptr<Program> res(new Program(name, std::move(globals), std::move(locals), std::move(init), std::move(funs)));
		return res;
	}

}

#include "observer.hpp"
