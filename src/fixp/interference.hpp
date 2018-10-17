#pragma once

#include <vector>
#include "cfg.hpp"
#include "post.hpp"
#include "encoding.hpp"
#include "fixpoint.hpp"

namespace tmr {

	/**
	 * @brief Computes all post-images resulting from an iterference step for the entire encoding.
	 * @details Depending on the ``msetup`` this computes interference for only Thread 1
	 *          or for Thread 1 and Thread 2.
	 */	
	void mk_all_interference(Encoding& enc, RemainingWork& work, const Program& prog);

}