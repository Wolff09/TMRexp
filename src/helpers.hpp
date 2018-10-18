#pragma once

#include <functional>
#include "prog.hpp"
#include "shape.hpp"
#include "cfg.hpp"


namespace tmr {

	static const constexpr RelSet EQ_            = RelSet(1);  // contains: =
	static const constexpr RelSet MT_            = RelSet(2);  // contains: ↦
	static const constexpr RelSet MF_            = RelSet(4);  // contains: ↤
	static const constexpr RelSet GT_            = RelSet(8);  // contains: ⇢
	static const constexpr RelSet GF_            = RelSet(16); // contains: ⇠
	static const constexpr RelSet BT_            = RelSet(32); // contains: ⋈
	static const constexpr RelSet EQ_BT          = RelSet(33); // contains: =, ⋈
	static const constexpr RelSet GT_GF          = RelSet(24); // contains: ⇢, ⇠
	static const constexpr RelSet MF_GF          = RelSet(20); // contains: ↤, ⇠
	static const constexpr RelSet MT_BT          = RelSet(34); // contains: ↦, ⋈
	static const constexpr RelSet MT_GF          = RelSet(18); // contains: ↦, ⇠
	static const constexpr RelSet MT_GT          = RelSet(10); // contains: ↦, ⇢
	static const constexpr RelSet GT_BT          = RelSet(40); // contains: ⇢, ⋈
	static const constexpr RelSet EQ_MT_GT       = RelSet(11); // contains: =, ↦, ⇢
	static const constexpr RelSet EQ_MF_GF       = RelSet(21); // contains: =, ↤, ⇠
	static const constexpr RelSet EQ_GT_GF       = RelSet(25); // contains: =, ⇢, ⇠
	static const constexpr RelSet MF_GF_BT       = RelSet(52); // contains: ↤, ⇠, ⋈
	static const constexpr RelSet MT_GT_GF       = RelSet(26); // contains: ↦, ⇢, ⇠
	static const constexpr RelSet MT_GT_BT       = RelSet(42); // contains: ↦, ⇢, ⋈
	static const constexpr RelSet EQ_MT_GT_BT    = RelSet(43); // contains: =, ↦, ⇢, ⋈
	static const constexpr RelSet EQ_MT_MF_GT_GF = RelSet(31); // contains: =, ↦, ↤, ⇢, ⇠
	static const constexpr RelSet EQ_GT_MF_GF_BT = RelSet(61); // contains: =, ↤, ⇢, ⇠, ⋈
	static const constexpr RelSet MT_GT_MF_GF_BT = RelSet(62); // contains: ↤, ⇢, ⇠, ⋈
	static const constexpr RelSet PRED           = RelSet(63); // contains: =, ↦, ↤, ⇢, ⇠, ⋈

	static std::size_t mk_var_index(const Shape& shape, const Variable& var, const unsigned short tid) {
		if (var.global()) return shape.index_GlobalVar(var);
		else return shape.index_LocalVar(var, tid);
	}

	static std::size_t mk_var_index(const Shape& shape, const Expr& expr, const unsigned short tid) {
		switch (expr.clazz()) {
			case Expr::VAR: return mk_var_index(shape, ((const VarExpr&) expr).decl(), tid);
			case Expr::SEL: return mk_var_index(shape, ((const Selector&) expr).decl(), tid);
			case Expr::NIL: return shape.index_NULL();
		}
	}

	/**
	 * @brief Checks whether the passed relation relating x and z is consistent.
	 *        (Implements the consistent(x,z,~) predicate.)
	 */
	bool consistent(const Shape& shape, std::size_t x, std::size_t z, Rel rel);

	/**
	 * @brief Checks whether the given shape is consistent.
	 * @details A full consistency check is rather expensive and is present for testing purpose only.
	 *          It runs in O(N^3) with N=shape.size().
	 *          Hence, this function is marked as deprecated.
	 */
	bool consistent(const Shape& shape);


	/**
	 * @brief Checks whether the passed shape contains all predicates that may follow from reflexivity and transitivity.
	 * @details This check is rather expensive and is present for testing purpose only.
	 *          It runs in O(N^3) with N=shape.size().
	 *          Hence, this function is marked as deprecated.
	 */
	bool is_closed_under_reflexivity_and_transitivity(const Shape& shape, bool weak=false);

	/**
	 * @brief Gives either a consistent shape where the cell determined by ``row`` and ``col``
	 *        is reduced to contain at most the elements from ``match`` (intersection)
	 *        and all other cells are a subset of the given ``shape`` or it gives NULL.
	 *        (If a shape is returned that shape is a partial concretisation of the input shape)
	 */
	Shape* isolate_partial_concretisation(const Shape& shape, const std::size_t row, const std::size_t col, const RelSet match);

	/**
	 * @brief Removes inconsistent relations in the given shape.
	 * @details Returns false iff. the given shape contains definitely inconsistent relations.
	 */
	bool make_concretisation(Shape& shape);

	/**
	 * @brief Disambiguates the reachability information about the passed variable.
	 * @details For a passed variable x, any cell relating x with another cell term
	 *          is split such that it contains either {=} or {↦,⇢} or {↤,⇠} or {⋈}
	 *          (or subsets of those).
	 */
	std::vector<Shape*> disambiguate(const Shape& shape, const std::size_t index);

	/**
	 * @brief Disambiguates the reachability information about the passed cell.
	 * @details The passed shape is split up such that in the resulting shapes the
	 *          cell contains either {=} or {↦,⇢} or {↤,⇠} or {⋈} (or subsets of those).
	 */
	std::vector<Shape*> disambiguate_cell(const Shape& shape, const std::size_t row, const std::size_t col);

	/**
	 * @brief Merges shapes yielding a fresh one. If the provided list is empty, NULL is returned.
	 */
	Shape* merge(std::vector<Shape*>& shapes);

	/**
	 * @brief Gives a list of cell term indexes t such that x~t holds where x is the
	 *        given index and ~ is a member of the passed set of relations.
	 *        in relation with
	 */
	std::vector<std::size_t> get_related(const Shape& shape, std::size_t x, RelSet anyOf);

	/**
	 * @brief Relates all elements from the first given vector with all elements from the second given vector.
	 *        Relating here means that for every pair x,y with x∈vec1 and y∈vec2 the relation x~y with ~=rel
	 *        is added and x⋈y is removed.
	 */
	void relate_all(Shape& shape, const std::vector<std::size_t>& vec1, const std::vector<std::size_t>& vec2, Rel rel);

	/**
	 * @brief Adds the given relation to the set of current relations relating all pairs of elements from
	 *        first and second vector. That is, for all pairs x,y with x∈vec1 and y∈vec2 we set
	 *        ``shape[x][y] := shape[x][y] + rel``.
	 */
	void extend_all(Shape& shape, const std::vector<std::size_t>& vec1, const std::vector<std::size_t>& vec2, Rel rel);

	/**
	 * @brief Removes all successors of x w.r.t. ↦ and ⇢.
	 *        Removes all predicates that rely on the removed relations.
	 */
	void remove_successors(Shape& shape, std::size_t x);

}
