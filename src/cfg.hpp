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

	enum class DataSet { WITH_DATA, WITHOUT_DATA };
	std::ostream& operator<<(std::ostream& os, const DataSet& val);


	template<typename T, std::size_t N>
	class MultiStore {
		private:
			typedef std::array<T, N> __store__;
			__store__ _vals;

		public:
			MultiStore() {}
			MultiStore(__store__ inout) : _vals(inout) {}
			MultiStore(T fill) { _vals.fill(fill); }
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
			// TODO: operator<<
	};

	typedef MultiStore<const Statement*, 3> MultiPc;
	typedef MultiStore<DataValue, 3> MultiInOut;
	typedef MultiStore<DataSet, 3> MultiSet;
	typedef MultiStore<bool, 3> MultiBool;

	static const DataValue DEFAULT_DATA_VALUE = DataValue::OTHER;
	static const DataSet DEFAULT_DATA_SET = DataSet::WITHOUT_DATA;


	struct Cfg {
		MultiPc pc;
		MultiState state; // observer state
		MultiInOut arg; // argument (value) of current function
		MultiBool offender;
		std::unique_ptr<Shape> shape;
		DataValue datasel0; // data0 selectors for special pointer
		DataValue datasel1; // data1 selectors for special pointer
		MultiSet dataset0;
		MultiSet dataset1;
		MultiSet dataset2;

		Cfg(std::array<const Statement*, 3> pc, MultiState state, Shape* shape)
		    : pc(pc), state(state), arg(DEFAULT_DATA_VALUE), offender(false), shape(shape),
		      datasel0(DEFAULT_DATA_VALUE), datasel1(DEFAULT_DATA_VALUE),
		      dataset0(DEFAULT_DATA_SET), dataset1(DEFAULT_DATA_SET), dataset2(DEFAULT_DATA_SET)
		{}
		Cfg(const Cfg& cfg, Shape* shape)
		    : pc(cfg.pc), state(cfg.state), arg(cfg.arg), offender(cfg.offender), shape(shape), datasel0(cfg.datasel0),
		      datasel1(cfg.datasel1), dataset0(cfg.dataset0), dataset1(cfg.dataset1), dataset2(cfg.dataset2)
		{}
		Cfg copy() const;
	};

	std::ostream& operator<<(std::ostream& os, const Cfg& cfg);

}
