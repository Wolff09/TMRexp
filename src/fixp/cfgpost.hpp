#pragma once

#include <vector>
#include "prog.hpp"
#include "cfg.hpp"
#include "post.hpp"

namespace tmr {
	
	/**
	 * @brief Computes all post-images for the given configuration.
	 * @details Depending on the ``msetup`` this computes post for only Thread 1
	 *          or for Thread 1 and Thread 2.
	 */
	std::vector<Cfg> mk_all_post(const Cfg& cfg, const Program& prog);

}