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


	enum class EpochValue { ZERO, ONE, TWO };
	std::ostream& operator<<(std::ostream& os, const EpochValue& val);


	template<typename T, T D>
	class SelectorStore {
		private:
			std::vector<T> _values;

		public:
			SelectorStore(const Shape& shape) : _values(shape.size(), D) {}
			SelectorStore(const Shape* shape) : _values(shape->size(), D) {}
			void set(std::size_t index, T value) { _values.at(index) = value; }
			T at(std::size_t index) const { return _values.at(index); }
			void print(std::ostream& os) const;
	};

	template<typename T, T D>
	static std::ostream& operator<<(std::ostream& os, const SelectorStore<T, D>& store) {
		store.print(os);
		return os;
	}

	typedef SelectorStore<DataValue, DataValue::OTHER> DataStore;
	typedef SelectorStore<EpochValue, EpochValue::ZERO> EpocheStore;
	static const MultiInOut DEFAULT_ARG = {{ DataValue::OTHER, DataValue::OTHER, DataValue::OTHER }};


	struct Cfg {
		MultiPc pc; // program counter
		MultiState state; // observer state
		MultiInOut arg; // argument (value) of current function, per thread
		std::unique_ptr<Shape> shape;
		DataStore datasel;
		EpocheStore epochesel;

		Cfg(std::array<const Statement*, 3> pc, MultiState state, Shape* shape) : pc(pc), state(state), arg(DEFAULT_ARG), shape(shape), datasel(shape), epochesel(shape) {}
		Cfg(const Cfg& cfg, Shape* shape) : pc(cfg.pc), state(cfg.state), arg(cfg.arg), shape(shape), datasel(cfg.datasel), epochesel(cfg.epochesel) {
			assert(cfg.shape->size() == shape->size());
		}
		Cfg copy() const;
	};

	std::ostream& operator<<(std::ostream& os, const Cfg& cfg);

}
