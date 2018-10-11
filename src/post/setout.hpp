#pragma once

#include <vector>
#include "../cfg.hpp"
#include "../helpers.hpp"
#include "post/helpers.hpp"

namespace tmr {

	static std::vector<Cfg> set_inout_output_value(const Cfg& cfg, std::size_t var, unsigned short tid, Shape* base_shape=NULL) {
		const Observer& observer = cfg.state.observer();
		Shape* remaining = base_shape != NULL ? base_shape : new Shape(*cfg.shape);
		std::vector<Cfg> result;

		for (std::size_t i = 0; i < remaining->sizeObservers(); i++) {
			std::size_t cti = remaining->index_ObserverVar(i);

			Shape* eq = isolate_partial_concretisation(*remaining, var, cti, EQ_);
			if (eq != NULL) {
				// var = cti => emit out(i);
				result.push_back(mk_next_config(cfg, eq, tid));
				result.back().inout[tid] = observer.mk_var(i);

				auto tmp = remaining;
				remaining = isolate_partial_concretisation(*tmp, var, cti, MT_GT_MF_GF_BT);
				delete tmp;
				if (remaining == NULL) break;
			}
		}

		if (remaining != NULL) {
			result.push_back(mk_next_config(cfg, remaining, tid));
			result.back().inout[tid] = OValue::Anonymous();
		}

		return result;
	}

}
