#include "encoding.hpp"


using namespace tmr;


/******************************** SORTING ********************************/

bool cfg_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	// std::cout << "- comparing:" << std::endl << "    " << lhs << "    " << rhs;
	assert(lhs.shape != NULL);
	assert(rhs.shape != NULL);
	assert(lhs.shape->size() == rhs.shape->size());
	assert(lhs.ages->size() == rhs.ages->size());

	// compare trivial stuff
	// if (lhs.state < rhs.state) return true;
	// if (rhs.state < lhs.state) return false;

	// age field comparsion
	for (std::size_t col = lhs.shape->offset_locals(0); col < lhs.ages->size(); col++)
		for (std::size_t row = 0; row < col; row++)
			for (bool bc : {false, true})
				for (bool br : {false, true}) {
					auto lij = lhs.ages->at(row, br, col, bc).type();
					auto rij = rhs.ages->at(row, br, col, bc).type();
					if (lij < rij) return true;
					else if (lij > rij) return false;
				}

	// compare remaining trivial stuff
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

	// if rhs is not smaller, return false
	return false;
}

bool key_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	// std::cout << "- comparing:" << std::endl << "    " << lhs << "    " << rhs;
	assert(lhs.shape != NULL);
	assert(rhs.shape != NULL);
	assert(lhs.shape->size() == rhs.shape->size());
	assert(lhs.ages->size() == rhs.ages->size());

	// state
	if (lhs.state < rhs.state) return true;
	if (rhs.state < lhs.state) return false;

	// global age field
	for (std::size_t col = 0; col < lhs.shape->offset_locals(0); col++)
		for (std::size_t row = 0; row < col; row++)
			for (bool bc : {false, true})
				for (bool br : {false, true}) {
					auto lij = lhs.ages->at(row, br, col, bc).type();
					auto rij = rhs.ages->at(row, br, col, bc).type();
					if (lij < rij) return true;
					else if (lij > rij) return false;
				}

	return false;
}


/******************************** ENCODING TAKE ********************************/

std::pair<bool, const Cfg&> Encoding::take(Cfg&& new_cfg) {
	assert(new_cfg.shape);
	assert(consistent(*new_cfg.shape));

	__store__::iterator pos = _map.find(new_cfg);

	if (pos == _map.end()) {
		// add new mapping
		Cfg key = new_cfg.copy();
		std::set<Cfg, cfg_comparator> nv;
		nv.insert(std::move(new_cfg));
		std::pair<Cfg, std::set<Cfg, cfg_comparator>> nkvp = std::make_pair(std::move(key), std::move(nv));
		auto insert = _map.insert(std::move(nkvp));
		assert(insert.second);
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
			assert(cfg.pc == new_cfg.pc);
			assert(cfg.state == new_cfg.state);
			assert(cfg.ages == new_cfg.ages);
			assert(cfg.inout == new_cfg.inout);
			assert(cfg.seen == new_cfg.seen);
			// std::cerr << "encoding augments itself" << std::endl;

			bool updated = false;
			Shape& dst = *cfg.shape;
			Shape& src = *new_cfg.shape;
			assert(consistent(dst));
			assert(consistent(src));
			assert(cfg.pc == new_cfg.pc);
			assert(dst.size() == src.size());
			for (std::size_t row = 0; row < dst.size(); row++) {
				for (std::size_t col = row; col < dst.size(); col++) {
					auto both = setunion(dst.at(row, col), src.at(row, col));
					if (both != dst.at(row, col)) {
						dst.set(row, col, both);
						updated = true;
					}
				}
			}
			assert(consistent(*cfg.shape));

			for (std::size_t i = 0; i < dst.size(); i++) {
				bool new_sin = cfg.sin[i] || new_cfg.sin[i];
				if (cfg.sin[i] != new_sin) {
					updated = true;
					cfg.sin[i] = new_sin;
				}

				bool new_invalid = cfg.invalid[i] || new_cfg.invalid[i];
				if (cfg.invalid[i] != new_invalid) {
					updated = true;
					cfg.invalid[i] = new_invalid;
				}

				bool new_own = cfg.own.is_owned(i) && new_cfg.own.is_owned(i);
				if (cfg.own.is_owned(i) != new_own) {
					updated = true;
					cfg.own.set_ownership(i, new_own);
				}
			}

			return { updated, cfg };
		}
	}
}
