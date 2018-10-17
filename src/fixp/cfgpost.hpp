#pragma once

#include <vector>
#include "prog.hpp"
#include "cfg.hpp"
#include "post.hpp"

namespace tmr {
	
	/**
	 * @brief Computes all post-images for the given configuration.
	 */
	std::vector<Cfg> mk_all_post(const Cfg& cfg, const Program& prog);

	std::vector<Cfg> mk_all_post(const Cfg& cfg, unsigned short tid, const Program& prog);

}