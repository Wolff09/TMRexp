#pragma once

#include <array>
#include <memory>
#include <vector>
#include <set>
#include <map>
#include "prog.hpp"
#include "shape.hpp"
#include "cfg.hpp"
#include "helpers.hpp"


namespace tmr {

	struct cfg_comparator {
		bool operator() (const Cfg& lhs, const Cfg& rhs) const;
	};

	struct key_comparator {
		bool operator() (const Cfg& lhs, const Cfg& rhs) const;
	};

	class Encoding {
		public:
			typedef std::set<Cfg, cfg_comparator> __sub__store__;
			typedef std::map<Cfg, __sub__store__, key_comparator> __store__;
			
		private:
			__store__ _map;

		public:
			__store__::iterator begin() { return _map.begin(); }
			__store__::iterator end() { return _map.end(); }
			std::size_t size() const {
				std::size_t size = 0;
				for (const auto& kvp : _map) size += kvp.second.size();
				return size;
			}

			/**
			 * @brief Extends the encoding with the given configuration.
			 * 
			 * @param cfg configuration to add
			 * @return Gives a pair where member ``first`` is set to ``true`` when the encoding
			 *         was updated. That is the case if the given configurations was not present
			 *         before the call.
			 *         Member ``second`` is the the configuration which was augmented with the
			 *         given one.
			 */
			std::pair<bool, const Cfg&> take(Cfg&& new_cfg);
	};

}