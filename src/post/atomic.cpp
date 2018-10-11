#include "post.hpp"

#include <deque>

using namespace tmr;


std::vector<Cfg> tmr::post(const Cfg& cfg, const Atomic& stmt, unsigned short tid){
	std::vector<Cfg> result;
	result.reserve(4);

	std::deque<Cfg> work;

	work.push_back(cfg.copy());
	work.back().pc[tid] = stmt.sqz().next();

	while (!work.empty()) {
		if (work.back().pc[tid] == NULL) {
			// cfg has gone through the entire atomic block
			result.push_back(std::move(work.back()));
			// set proper next pc
			result.back().pc[tid] = stmt.next();
			work.pop_back();
		} else {
			// cfg is still in atomic block, so execute pc[tid]
			auto postimg = tmr::post(work.back(), tid);
			work.pop_back();
			for (Cfg& pi : postimg) work.push_back(std::move(pi));
		}
	}

	return result;
}
