#include "encoding.hpp"


using namespace tmr;


/******************************** SORTING ********************************/

bool cfg_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	// compare trivial stuff
	// if (lhs.state < rhs.state) return true;
	// if (rhs.state < lhs.state) return false;
	if (lhs.pc < rhs.pc) return true;
	if (rhs.pc < lhs.pc) return false;
	if (lhs.inout < rhs.inout) return true;
	if (rhs.inout < lhs.inout) return false;
	if (lhs.seen < rhs.seen) return true;
	if (rhs.seen < lhs.seen) return false;
	// if (lhs.own < rhs.own) return true;
	// if (lhs.own > rhs.own) return false;
	// if (lhs.sin < rhs.sin) return true;
	// if (lhs.sin > rhs.sin) return false;
	// if (lhs.invalid < rhs.invalid) return true;
	// if (lhs.invalid > rhs.invalid) return false;
	if (lhs.oracle < rhs.oracle) return true;
	if (rhs.oracle < lhs.oracle) return false;
	if (lhs.smrstate < rhs.smrstate) return true;
	if (rhs.smrstate < lhs.smrstate) return false;

	// if rhs is not smaller, return false
	return false;
}

bool key_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	// state
	if (lhs.state < rhs.state) return true;
	if (rhs.state < lhs.state) return false;

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

			for (std::size_t i = dst.offset_locals(0); i < dst.size(); i++) {
				bool new_valid = cfg.valid.at(i) && new_cfg.valid.at(i);
				if (cfg.valid.at(i) != new_valid) {
					updated = true;
					cfg.valid.set(i, new_valid);
				}

				bool new_own = cfg.own.at(i) && new_cfg.own.at(i);
				if (cfg.own.at(i) != new_own) {
					updated = true;
					cfg.own.set(i, new_own);
				}
			}

			return { updated, cfg };
		}
	}
}
