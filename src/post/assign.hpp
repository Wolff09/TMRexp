#pragma once

#include <vector>
#include "../prog.hpp"
#include "../shape.hpp"
#include "../cfg.hpp"
#include "../post.hpp"

namespace tmr {
	std::vector<Cfg> post_assignment_pointer(const Cfg& cfg, const Expr& lhs, const Expr& rhs, unsigned short tid, const Statement* spr_origin=NULL);
	std::vector<Cfg> post_assignment_data(   const Cfg& cfg, const Expr& lhs, const Expr& rhs, unsigned short tid, const Statement* spr_origin=NULL);

	std::vector<Cfg> post_assignment_pointer_var_var(  const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* spr_origin=NULL);
	std::vector<Cfg> post_assignment_pointer_var_next( const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* spr_origin=NULL);
	std::vector<Cfg> post_assignment_pointer_next_var( const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* spr_origin=NULL);
	std::vector<Cfg> post_assignment_pointer_next_next(const Cfg& cfg, const std::size_t lhs, const std::size_t rhs, unsigned short tid, const Statement* spr_origin=NULL);
	
	Shape* post_assignment_pointer_shape_var_var(  const Shape& input, const std::size_t lhs, const std::size_t rhs, const Statement* spr_origin=NULL);
	Shape* post_assignment_pointer_shape_var_next( const Shape& input, const std::size_t lhs, const std::size_t rhs, const Statement* spr_origin=NULL);
	Shape* post_assignment_pointer_shape_next_var( const Shape& input, const std::size_t lhs, const std::size_t rhs, const Statement* spr_origin=NULL);
}
