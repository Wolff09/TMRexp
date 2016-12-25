#pragma once

#include <array>
#include <memory>
#include <assert.h>
#include <iostream>
#include "prog.hpp"
#include "shape.hpp"
#include "observer.hpp"

#include <set>

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

	typedef MultiStore<const Statement*, 2> MultiPc;
	typedef MultiStore<OValue, 3> MultiInOut;
	typedef MultiStore<bool, 3> MultiOracle;
	typedef MultiStore<bool, 2> SeenOV;

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


	struct Cfg {
		MultiPc pc;
		MultiState state;
		MultiInOut inout;
		MultiOracle oracle;
		std::unique_ptr<Shape> shape;
		SeenOV seen;
		mutable Ownership own;


		Cfg(MultiPc pc, MultiState state, Shape* shape, MultiInOut inout)
		  : pc(pc), state(state), inout(inout), shape(shape),
		    own(shape->offset_vars(), shape->offset_program_vars(), shape->offset_locals(0), shape->size() + shape->sizeLocals()) {

			for (std::size_t i = 0; i < seen.size(); i++) seen[i] = false;
			for (std::size_t i = 0; i < oracle.size(); i++) oracle[i] = true;
		}
		Cfg(const Cfg& cfg, Shape* shape) : pc(cfg.pc), state(cfg.state), inout(cfg.inout), oracle(cfg.oracle), shape(shape), seen(cfg.seen), own(cfg.own) {}

		Cfg copy() const;
	};

	std::ostream& operator<<(std::ostream& os, const Cfg& cfg);

}