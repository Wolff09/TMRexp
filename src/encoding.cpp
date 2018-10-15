#include "encoding.hpp"

#include "config.hpp"

using namespace tmr;


/******************************** SORTING ********************************/

bool cfg_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	if (lhs.pc < rhs.pc) return true;
	if (rhs.pc < lhs.pc) return false;
	if (lhs.arg < rhs.arg) return true;
	if (rhs.arg < lhs.arg) return false;
	return false;
}

bool key_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	if (lhs.state0 < rhs.state0) return true;
	if (rhs.state0 < lhs.state0) return false;
	if (lhs.state1 < rhs.state1) return true;
	if (rhs.state1 < lhs.state1) return false;
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
		// std::cerr << "encoding ate that cfg" << std::endl;
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
			// std::cerr << "encoding augments itself" << std::endl;

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

			return { updated, cfg };
		}
	}
}
