#pragma once

#include <stdexcept>
#include <assert.h>
#include "relset.hpp"
#include "shape.hpp"
#include "../helpers.hpp"


namespace tmr {

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
