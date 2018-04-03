#include "encoding.hpp"

#include "config.hpp"

using namespace tmr;


/******************************** SORTING ********************************/

bool cfg_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	if (lhs.pc < rhs.pc) return true;
	if (rhs.pc < lhs.pc) return false;
	if (lhs.inout < rhs.inout) return true;
	if (rhs.inout < lhs.inout) return false;
	if (lhs.oracle < rhs.oracle) return true;
	if (rhs.oracle < lhs.oracle) return false;
	if (lhs.guard0state < rhs.guard0state) return true;
	if (rhs.guard0state < lhs.guard0state) return false;
	if (lhs.guard1state < rhs.guard1state) return true;
	if (rhs.guard1state < lhs.guard1state) return false;

	#if !MERGE_VALID_PTR
		// more precision
		if (lhs.valid_ptr < rhs.valid_ptr) return true;
		if (rhs.valid_ptr < lhs.valid_ptr) return false;
	#endif

	return false;
}

bool key_comparator::operator() (const Cfg& lhs, const Cfg& rhs) const{
	if (lhs.freed < rhs.freed) return true;
	if (rhs.freed < lhs.freed) return false;
	if (lhs.retired < rhs.retired) return true;
	if (rhs.retired < lhs.retired) return false;
	if (lhs.state < rhs.state) return true;
	if (rhs.state < lhs.state) return false;
	if (lhs.seen < rhs.seen) return true;
	if (rhs.seen < lhs.seen) return false;

	#if AD_HOC_PRECISION
		if (lhs.shape->at(5,6).to_ulong() < rhs.shape->at(5,6).to_ulong()) return true;
		if (rhs.shape->at(5,6).to_ulong() < lhs.shape->at(5,6).to_ulong()) return false;
	#else
		auto begin = lhs.shape->offset_program_vars();
		auto end = lhs.shape->offset_locals(0);
		for (std::size_t i = begin; i < end; i++) {
			for (std::size_t j = i+1; j < end; j++) {
				if (lhs.shape->at(i, j).to_ulong() < rhs.shape->at(i, j).to_ulong()) return true;
				if (rhs.shape->at(i, j).to_ulong() < lhs.shape->at(i, j).to_ulong()) return false;
			}
		}
	#endif

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
				#if MERGE_VALID_PTR
					bool new_valid_ptr = cfg.valid_ptr.at(i) && new_cfg.valid_ptr.at(i);
					if (cfg.valid_ptr.at(i) != new_valid_ptr) {
						updated = true;
						cfg.valid_ptr.set(i, new_valid_ptr);
					}
				#endif
				bool new_valid_next = cfg.valid_next.at(i) && new_cfg.valid_next.at(i);
				if (cfg.valid_next.at(i) != new_valid_next) {
					updated = true;
					cfg.valid_next.set(i, new_valid_next);
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
