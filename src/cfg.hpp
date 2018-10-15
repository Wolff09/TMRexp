#pragma once

#include <array>
#include <memory>
#include <assert.h>
#include <iostream>
#include "prog.hpp"
#include "shape.hpp"
#include "observer.hpp"
#include "config.hpp"

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

	typedef MultiStore<const Statement*, 3> MultiPc;
	typedef MultiStore<DataValue, 3> MultiInOut;


	struct Cfg {
		MultiPc pc; // program counter for threads
		MultiState state0; // observer state, index 0 (e.g. hazard pointer 0)
		MultiState state1; // observer state, index 1 (e.g. hazard pointer 0)
		MultiInOut arg; // argument (value) of current function
		std::unique_ptr<Shape> shape;

		Cfg(std::array<const Statement*, 3> pc, MultiState state0, MultiState state1, Shape* shape) : pc(pc), state0(state0), state1(state1), shape(shape) {}
		Cfg(const Cfg& cfg, Shape* shape) : pc(cfg.pc), state0(cfg.state0), state1(cfg.state1), arg(cfg.arg), shape(shape) {}
		Cfg copy() const;
	};

	std::ostream& operator<<(std::ostream& os, const Cfg& cfg);

}
