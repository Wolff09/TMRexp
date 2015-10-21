#include "post.hpp"

using namespace tmr;


std::vector<Cfg> tmr::post(const Cfg& cfg, const Atomic& stmt, unsigned short tid, MemorySetup msetup){
	// std::cout << "ATOMIC BLOCK" << std::endl;

	std::vector<Cfg> result;
	std::vector<Cfg> work;

	work.push_back(cfg.copy());
	work.back().pc[tid] = stmt.sqz().next();
	assert(work.back().pc[tid] == &stmt.sqz().at(0));

	while (!work.empty()) {
		if (work.back().pc[tid] == NULL) {
			// cfg has gone through the entire atomic block
			result.push_back(std::move(work.back()));
			// set proper next pc
			result.back().pc[tid] = stmt.next();
			work.pop_back();
		} else {
			// cfg is still in atomic block, so execute pc[tid]
			auto postimg = tmr::post(work.back(), tid, msetup);
			work.pop_back();
			// std::cout << "...pi1... " << postimg.size() << std::endl << *postimg.back().shape;
			for (Cfg& pi : postimg) work.push_back(std::move(pi));
		}
	}

	return result;
}
