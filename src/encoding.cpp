#include "encoding.hpp"

#include "config.hpp"

using namespace tmr;


/******************************** SORTING ********************************/

#define cmp(x, y) if (x < y) { return true; } if (y < x) { return false; };

bool cfg_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	cmp(lhs.pc, rhs.pc);
	cmp(rhs.arg, lhs.arg);
	cmp(lhs.offender, rhs.offender);
	cmp(rhs.dataset0, lhs.dataset0);
	cmp(rhs.dataset1, lhs.dataset1);
	cmp(rhs.dataset2, lhs.dataset2);
	cmp(lhs.threadstate[0], rhs.threadstate[0]);
	cmp(lhs.localepoch[0], rhs.localepoch[0]);
	return false;
}

bool key_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	cmp(lhs.smrstate, rhs.smrstate);
	cmp(rhs.datasel0, lhs.datasel0);
	cmp(rhs.datasel1, lhs.datasel1);
	cmp(rhs.globalepoch, lhs.globalepoch);
	cmp(rhs.epochsel, lhs.epochsel);
	return false;
}


/******************************** ENCODING TAKE ********************************/

std::pair<bool, const Cfg&> Encoding::take(Cfg&& new_cfg) {
	__store__::iterator pos = _map.find(new_cfg);

	if (pos == _map.end()) {
		// add new mapping
		Cfg key = new_cfg.copy();
		std::set<Cfg, cfg_comparator> nv;
		nv.insert(std::move(new_cfg));
		std::pair<Cfg, std::set<Cfg, cfg_comparator>> nkvp = std::make_pair(std::move(key), std::move(nv));
		auto insert = _map.insert(std::move(nkvp));
		auto& inserted_pair = *insert.first;
		return { true, *inserted_pair.second.begin() };

	} else {
		// merge new_cfg.shape into cfg.shape and check for updates
		auto subpos = pos->second.find(new_cfg);
		if (subpos == pos->second.end()) {
			auto insert = pos->second.insert(std::move(new_cfg));
			return { true, *insert.first };

		} else {
			const Cfg& cfg = *subpos;
			// merge shapes
			bool updated = false;
			Shape& dst = *cfg.shape;
			Shape& src = *new_cfg.shape;
			for (std::size_t row = 0; row < dst.size(); row++) {
				for (std::size_t col = row; col < dst.size(); col++) {
					auto both = setunion(dst.at(row, col), src.at(row, col));
					if (both != dst.at(row, col)) {
						dst.set(row, col, both);
						updated = true;
					}
				}
			}

			// merge ownership
			for (std::size_t i : { 0, 1}) {
				if (cfg.owned[i] != new_cfg.owned[i]) {
					cfg.owned[i] = false;
					updated = true;
				}
			}

			return { updated, cfg };
		}
	}
}
