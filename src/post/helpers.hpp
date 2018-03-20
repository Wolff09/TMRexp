#pragma once

#include <stdexcept>
#include <assert.h>
#include "relset.hpp"
#include "shape.hpp"
#include "../helpers.hpp"


namespace tmr {

	static inline void ensure_prf(const Shape& shape, std::size_t var, const Statement* stmt=NULL) {
		// // var is an index which is accessed by an selector or by free
		// // check whether var points (can point) to free
		// if (shape.test(var, shape.index_FREE(), MT)) {
		//	std::cout << std::endl << "*****************************" << std::endl;
		//	std::cout << "Strong Pointer Race detected!" << std::endl << std::endl;
		//	std::cout << "while accessing cell term " << var << std::endl;
		//	if (stmt != NULL) std::cout << "in statement " << *stmt << std::endl;
		//	std::cout << "Shape:" << std::endl << shape << std::endl;
		  	
		//	Shape* iso = isolate_partial_concretisation(shape, var, shape.index_FREE(), MT_);
		//	std::cout << "Partial concretisation for shape[" << var << "][FREE]=MT is:" << std::endl;
		//	if (iso == NULL) std::cout << "not valid => false-positive pointer race" << std::endl;
		//	else std::cout << *iso << std::endl;

		//	throw std::runtime_error("Strong Pointer Race detected while accessing cell term with id=" + std::to_string(var) + ".");
		// }

		// TODO: what to do here?
	}

	/*static inline bool is_invalid(const Shape& shape, std::size_t var) {
		return shape.test(var, shape.index_FREE(), MT);
	}*/

	static inline bool is_valid(const Cfg& cfg, std::size_t var) {
		return cfg.valid.at(var);
	}

	static inline bool is_invalid(const Cfg& cfg, std::size_t var) {
		return !is_valid(cfg, var);
	}

	static inline void raise_epr(const Cfg& cfg, std::size_t var, std::string msg) {
		std::cout << std::endl;
		std::cout << "**************************************" << std::endl;
		std::cout << "'Almost' Relaxed Pointer Race detected" << std::endl;
		std::cout << msg << std::endl;
		std::cout << "for cell-id: " << var << std::endl;
		std::cout << "in: " << cfg << *cfg.shape << std::endl;
		throw std::runtime_error("'Almost' Relaxed Pointer Race detected.");
	}

	static inline void raise_rpr(const Cfg& cfg, std::size_t var, std::string msg) {
		std::cout << std::endl;
		std::cout << "*****************************" << std::endl;
		std::cout << "Relaxed Pointer Race detected" << std::endl;
		std::cout << msg << std::endl;
		std::cout << "for cell-id: " << var << std::endl;
		std::cout << "in: " << cfg << *cfg.shape << std::endl;
		throw std::runtime_error("Relaxed Pointer Race detected.");
	}

	static inline void ensure_prf(const Shape& shape, std::size_t var, const Statement& stmt) {
		ensure_prf(shape, var, &stmt);
	}

	static inline void check_ptr_access(const Shape& shape, std::size_t var, const Statement* stmt=NULL) {
		if (shape.test(var, shape.index_NULL(), EQ)) {
			std::cout << std::endl << "*******************" << std::endl;
			std::cout << "Dereferencing NULL!" << std::endl << std::endl;
			if (stmt != NULL) std::cout << "in statement: " << *stmt << std::endl;
			std::cout << "while accessing cell term " << var << std::endl;
			std::cout << "in shape " << std::endl << shape << std::endl;

			Shape* iso = isolate_partial_concretisation(shape, var, shape.index_NULL(), EQ_);
			std::cout << "Partial concretisation for " << var << "=NULL is:" << std::endl;
			if (iso == NULL) std::cout << "not valid => false-positive" << std::endl;
			else std::cout << *iso << std::endl;

			throw std::runtime_error("Dereferencing NULL while accessing cell term with id=" + std::to_string(var) + ".");
		}
		if (shape.test(var, shape.index_UNDEF(), MT)) {
			std::cout << std::endl << "*********************************" << std::endl;
			std::cout << "Accessing uninitialized variable!" << std::endl << std::endl;
			if (stmt != NULL) std::cout << "in statement: " << *stmt << std::endl;
			std::cout << "while accessing cell term " << var << std::endl;
			std::cout << "in shape " << std::endl << shape << std::endl;
			throw std::runtime_error("Accessing uninitialized variable while accessing cell term with id=" + std::to_string(var) + ".");
		}
	}

	static inline void check_no_reachability(const Shape& shape, std::size_t x, std::size_t y, const Shape* stemsfrom=NULL, const Statement* stmt=NULL) {
		if (haveCommon(shape.at(x, y), EQ_MT_GT)) {
			std::cout << std::endl << "***************" << std::endl;
			std::cout << "Cycle detected!" << std::endl << std::endl;
			std::cout << "cycle between " << x << " and " << y << " in " << std::endl << shape << std::endl;
			if (stemsfrom != NULL) std::cout << "Stems from: " << std::endl << *stemsfrom << std::endl;
			if (stmt != NULL) std::cout << "in statement: " << *stmt << std::endl;
			throw std::runtime_error("Closed cycle detected between cell terms " + std::to_string(x) + " and " + std::to_string(y) + ".");
		}
	}

	// TODO: check for udef/seg access
	#define CHECK_RPRF(x) if ensure_prf(input, x);
	#define CHECK_RPRF_ws(x, stmt) ensure_prf(input, x, stmt);
	// TODO: check for null access
	#define CHECK_ACCESS(x) check_ptr_access(input, x);
	#define CHECK_ACCESS_ws(x, stmt) check_ptr_access(input, x, stmt);
	#define CHECK_NO_REACH(x, y) check_no_reachability(input, x, y);

	#define CHECK_STMT assert(cfg.pc[tid] == &stmt);

	static inline Cfg mk_next_config(const Cfg& blueprint, Shape* shape, const Statement* next, unsigned short tid) {
		Cfg res(blueprint, shape);
		res.pc[tid] = next;
		return res;
	}

	static inline Cfg mk_next_config(const Cfg& blueprint, Shape* shape, unsigned short tid) {
		assert(blueprint.pc[tid] != NULL);
		Cfg res(blueprint, shape);
		res.pc[tid] = blueprint.pc[tid]->next();
		return res;
	}

	static inline std::vector<Cfg> mk_next_config_vec(const Cfg& blueprint, Shape* shape, unsigned short tid) {
		std::vector<Cfg> result;
		result.push_back(mk_next_config(blueprint, shape, tid));
		return result;
	}

}
