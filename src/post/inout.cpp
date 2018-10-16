#include "post.hpp"

#include <assert.h>
#include "../helpers.hpp"
#include "post/helpers.hpp"
#include "post/assign.hpp"

using namespace tmr;



/******************************** INPUT: LHS.DATA = __in__ ********************************/



std::vector<Cfg> tmr::post(const Cfg& cfg, const ReadInputAssignment& stmt, unsigned short tid) {
	CHECK_STMT;

	const Expr& le = stmt.expr();
	const Shape& input = *cfg.shape;
	std::size_t lhs = mk_var_index(input, stmt.expr(), tid);
	DataValue new_data = cfg.arg[tid];

	if (le.type() != DATA) {
		throw std::logic_error("Unsupported ReadInputAssignment.");
	}

	// stmt: ptr->data = arg
	std::vector<Cfg> result;
	auto shapes = disambiguate(*cfg.shape, lhs);
	result.reserve(shapes.size());

	for (Shape* s : shapes) {
		result.push_back(mk_next_config(cfg, s, tid));
		Cfg& cf = result.back();
		for (std::size_t i = 0; i < s->size(); i++) {
			if (cf.shape->test(i, lhs, EQ)) {
				cf.datasel.set(i, new_data);
			}
		}
	}
	
	return result;
}
