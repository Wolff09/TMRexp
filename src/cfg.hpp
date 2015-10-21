#pragma once

#include <array>
#include <memory>
#include <assert.h>
#include <iostream>
#include "prog.hpp"
#include "shape.hpp"
#include "observer.hpp"

namespace tmr {

	template<typename T, std::size_t N>
	class MultiStore {
		private:
			typedef std::array<T, N> __store__;
			__store__ _vals;

		public:
			MultiStore() {}
			MultiStore(__store__ inout) : _vals(inout) {}
			std::size_t size() const { return _vals.size(); }
			bool operator==(const MultiStore& other) const { return _vals == other._vals; }
			bool operator!=(const MultiStore& other) const { return !(*this == other); }
			typename __store__::reference operator[](typename __store__::size_type index) { assert(index < _vals.size()); return _vals[index]; }
			typename __store__::const_reference operator[](typename __store__::size_type index) const { assert(index < _vals.size()); return _vals[index]; }
			bool operator<(const MultiStore& other) const {
				for (std::size_t i = 0; i < _vals.size(); i++)
					if (_vals[i] < other._vals[i]) return true;
					else if (other._vals[i] < _vals[i]) return false;
				return false;
			}
	};

	typedef MultiStore<const Statement*, 3> MultiPc;
	typedef MultiStore<OValue, 3> MultiInOut;
	typedef MultiStore<bool, 3> MultiOracle;
	typedef MultiStore<bool, 2> SeenOV;


	class AgeRel {
		public:
			enum Type { EQ=0, LT=1, GT=2, BOT=3 };
			AgeRel(Type type);
			operator Type() const;
			Type type() const;
			AgeRel symmetric() const;
		private:
			unsigned char _val;
	};

	class AgeMatrix {
		private:
			std::size_t _bounds;
			std::vector<AgeRel> _rels; // col store, right upper triangle, w/o diagonal, w/o NULL,FREE,UNDEF

		public:
			AgeMatrix(std::size_t numNonLocals, std::size_t numLocals, std::size_t numThreads);
			std::size_t size() const { return _bounds; }
			AgeRel at(std::size_t row, bool row_next, std::size_t col, bool col_next) const;
			inline AgeRel at_real(std::size_t row, std::size_t col) const { return at(row, false, col, false); }
			inline AgeRel at_next(std::size_t row, std::size_t col) const { return at(row, true, col, true); }
			void set(std::size_t row, bool row_next, std::size_t col, bool col_next, AgeRel rel);
			inline void set_real(std::size_t row, std::size_t col, AgeRel rel) { set(row, false, col, false, rel); }
			inline void set_next(std::size_t row, std::size_t col, AgeRel rel) { set(row, true, col, true, rel); }
			void extend(std::size_t numLocals);
			void shrink(std::size_t numLocals);
			bool operator==(const AgeMatrix& other) const;
	};

	class Ownership {
		private:
			std::size_t _first_obs;
			std::size_t _first_global;
			std::size_t _first_local;
			std::vector<bool> _bits;

		public:
			Ownership(std::size_t first_obs, std::size_t first_global, std::size_t first_local, std::size_t max_size);
			bool is_owned(std::size_t pos) const;
			void publish(std::size_t pos);
			void own(std::size_t pos);
			void set_ownership(std::size_t pos, bool val);
			bool operator<(const Ownership& other) const;
			bool operator>(const Ownership& other) const;
			void print(std::ostream& os) const;
	};

	class StrongInvalidity {
		private:
			std::vector<bool> _bits;

		public:
			StrongInvalidity(std::size_t max_size);
			bool is_strongly_invalid(std::size_t pos) const;
			bool at(std::size_t pos) const;
			void set(std::size_t pos, bool val);
			std::vector<bool>::reference operator[](std::size_t pos);
			std::vector<bool>::const_reference operator[](std::size_t pos) const;
			bool operator<(const StrongInvalidity& other) const;
			bool operator>(const StrongInvalidity& other) const;
			void print(std::ostream& os) const;
	};


	struct Cfg {
		MultiPc pc;
		MultiState state;
		MultiInOut inout;
		MultiOracle oracle;
		std::unique_ptr<Shape> shape;
		std::unique_ptr<AgeMatrix> ages;
		SeenOV seen;
		Ownership own;
		StrongInvalidity sin;

		Cfg(std::array<const Statement*, 3> pc, MultiState state, Shape* shape, AgeMatrix* ages, MultiInOut inout)
		  : pc(pc), state(state), inout(inout), shape(shape), ages(ages),
		    own(shape->offset_vars(), shape->offset_program_vars(), shape->offset_locals(0), shape->size() + shape->sizeLocals()),
		    sin(shape->size() + shape->sizeLocals()) {

			for (std::size_t i = 0; i < seen.size(); i++) seen[i] = false;
			for (std::size_t i = 0; i < oracle.size(); i++) oracle[i] = false;
		}
		Cfg(const Cfg& cfg, Shape* shape) : pc(cfg.pc), state(cfg.state), inout(cfg.inout), oracle(cfg.oracle), shape(shape), ages(new AgeMatrix(*cfg.ages)), seen(cfg.seen), own(cfg.own), sin(cfg.sin) {}

		Cfg copy() const;
	};

	std::ostream& operator<<(std::ostream& os, const AgeRel& r);
	std::ostream& operator<<(std::ostream& os, const AgeMatrix& m);
	std::ostream& operator<<(std::ostream& os, const Cfg& cfg);

}