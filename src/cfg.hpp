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

	typedef MultiStore<const Statement*, 3> MultiPc;
	typedef MultiStore<OValue, 3> MultiInOut;
	typedef MultiStore<bool, 3> MultiOracle;
	typedef MultiStore<bool, 2> SeenOV;


	template<typename T, T DEFAULT>
	class DynamicStore {
		private:
			std::size_t _offset;
			std::vector<T> _store;
		public:
			DynamicStore(std::size_t offset, std::size_t size) : _offset(offset), _store(size) {}
			DynamicStore(const T& prototype, std::size_t offset, std::size_t size) : _offset(offset), _store(size, prototype) {}
			T at(std::size_t index) const { return index < _offset ? DEFAULT : _store.at(index-_offset); }
			void set(std::size_t index, T value) {
				if (index >= _offset) {
					_store.at(index-_offset) = value;
				}
			}
			bool operator<(const DynamicStore<T, DEFAULT>& other) const {
				for (std::size_t i = 0; i < _store.size(); i++)
					if (_store[i] < other._store[i]) return true;
					else if (other._store[i] < _store[i]) return false;
				return false;
			}
			void print(std::ostream& os) const;
	};

	typedef DynamicStore<bool, false> DynamicOwnership;
	typedef DynamicStore<bool, true> DynamicValidity;
	typedef DynamicStore<const State*, nullptr> DynamicSMRState;


	struct Cfg {
		MultiPc pc;
		MultiState state;
		MultiInOut inout;
		MultiOracle oracle;
		std::unique_ptr<Shape> shape;
		SeenOV seen;
		mutable DynamicOwnership own;
		mutable DynamicValidity valid_ptr;
		mutable DynamicValidity valid_next;
		mutable DynamicSMRState guard0state;
		mutable DynamicSMRState guard1state;
		// TODO: realloc address smr state
		bool freed;
		bool retired;

		Cfg(std::array<const Statement*, 3> pc, MultiState linstate, Shape* shape/*, MultiInOut inout*/)
		  : pc(pc), state(linstate), /*inout(inout),*/ shape(shape),
		    own(false, shape->offset_locals(0), 2*shape->sizeLocals()),
		    valid_ptr(false, shape->offset_locals(0), 2*shape->sizeLocals()),
		    valid_next(false, shape->offset_locals(0), 2*shape->sizeLocals()),
		    guard0state(NULL, shape->offset_locals(0), 2*shape->sizeLocals()),
		    guard1state(NULL, shape->offset_locals(0), 2*shape->sizeLocals()),
		    freed(true), retired(false)
		{
			for (std::size_t i = 0; i < seen.size(); i++) seen[i] = false;
			for (std::size_t i = 0; i < oracle.size(); i++) oracle[i] = false;
		}
		Cfg(const Cfg& cfg, Shape* shape) : pc(cfg.pc), state(cfg.state), inout(cfg.inout), oracle(cfg.oracle), shape(shape), seen(cfg.seen), own(cfg.own), valid_ptr(cfg.valid_ptr), valid_next(cfg.valid_next), guard0state(cfg.guard0state), guard1state(cfg.guard1state), freed(cfg.freed), retired(cfg.retired) {}
		Cfg copy() const;
	};

	std::ostream& operator<<(std::ostream& os, const Cfg& cfg);

}
