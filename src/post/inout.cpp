#include "post.hpp"

#include <assert.h>
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/assign.hpp"

using namespace tmr;



/******************************** INPUT: LHS.DATA = __in__ ********************************/



std::vector<Cfg> tmr::post(const Cfg& cfg, const WriteRecData& stmt, unsigned short tid) {
	CHECK_STMT;

	if (!cfg.offender[tid]) {
		return mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);

	}

	auto result = mk_next_config_vec(cfg, new Shape(*cfg.shape), tid);
	
	DataValue data;
	switch (stmt.type()) {
		case WriteRecData::FROM_ARG: data = cfg.arg[tid]; break;
		case WriteRecData::FROM_NULL: data = DataValue::OTHER; break; // TODO: also add DataValue::DATA
	}
	
	if (cfg.offender[tid]) {
		switch (stmt.index()) {
			case 0: result.back().datasel0 = data; break;
			case 1: result.back().datasel1 = data; break;
			default: throw std::logic_error("Unsupported data selector.");
		}
	}

	return result;
}
