#pragma once

#include <vector>
#include "cfg.hpp"
#include "post.hpp"
#include "encoding.hpp"
#include "fixpoint.hpp"

namespace tmr {

	/**
	 * @brief Computes all post-images resulting from an iterference step for the entire encoding.
	 */	
	void mk_all_interference(Encoding& enc, RemainingWork& work);

	void mk_summary(RemainingWork& work, const Cfg& cfg, const Program& prog);

}