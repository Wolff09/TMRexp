#include "helpers.hpp"

#include <set>
#include <stack>

using namespace tmr;


/******************************** SHAPE ASSERTIONS ********************************/

bool check_special_relations_constraints(const Shape& shape) {
	auto null = shape.index_NULL();
	auto free = shape.index_FREE();
	auto undef = shape.index_UNDEF();
	// ensure that null and undef are unrelated
	if (shape.at(null, undef).count() != 1 || !shape.at(null, undef).test(BT)) return false;
	// ensure that null and free are unrelated
	if (shape.at(null, free).count() != 1 || !shape.at(null, free).test(BT)) return false;
	// ensure that undef and free are unrelated
	if (shape.at(undef, free).count() != 1 || !shape.at(undef, free).test(BT)) return false;
	for (std::size_t i = shape.offset_vars(); i < shape.size(); i++) {
		// cell term i may only relate with =, ↦, ⇢ or ⋈ to NULL
		if (shape.at(i, null).test(MF)) return false;
		if (shape.at(i, null).test(GF)) return false;
		// cell term i may only relate with ↦, ⇢ or ⋈ to FREE
		if (shape.at(i, free).test(EQ)) return false;
		if (shape.at(i, free).test(MF)) return false;
		if (shape.at(i, free).test(GF)) return false;
		// cell term i may only relate with ↦, ⇢ or ⋈ to UNDEFINED
		if (shape.at(i, free).test(EQ)) return false;
		if (shape.at(i, free).test(MF)) return false;
		if (shape.at(i, free).test(GF)) return false;
	}
	return true;
}


/******************************** CONSISTENCY ********************************/

bool consistentEQ(RelSet xy, RelSet yz) {
	return (xy & symmetric(yz)).any();
}

bool consistentMT(RelSet xy, RelSet yz) {
	if (xy.test(MT) && yz.test(EQ)) return true;
	if (xy.test(MF) && yz.test(GT)) return true;
	if (xy.test(GT) && (yz & MF_GF).any()) return true;
	if (xy.test(GF) && yz.test(GT)) return true;
	if (xy.test(EQ) && yz.test(MT)) return true;
	if (xy.test(BT) && (yz & MT_GT_BT).any()) return true;
	return false;
}

bool consistentMF(RelSet xy, RelSet yz) {
	return consistentMT(symmetric(yz), symmetric(xy));
}

bool consistentGT(RelSet xy, RelSet yz) {
	if (xy.test(MT) && (yz & MT_GT).any()) return true;
	if (xy.test(MF) && yz.test(GT)) return true;
	if (xy.test(GT) && (yz & EQ_MT_MF_GT_GF).any()) return true;
	if (xy.test(GF) && yz.test(GT)) return true;
	if (xy.test(EQ) && yz.test(GT)) return true;
	if (xy.test(BT) && (yz & MT_GT_BT).any()) return true;
	return false;
}

bool consistentGF(RelSet xy, RelSet yz) {
	return consistentGT(symmetric(yz), symmetric(xy));
}

bool consistentBT(RelSet xy, RelSet yz) {
	if (xy.test(MT) && (yz & MF_GF_BT).any()) return true;
	if (xy.test(MF) && yz.test(BT)) return true;
	if (xy.test(GT) && (yz & MF_GF_BT).any()) return true;
	if (xy.test(GF) && yz.test(BT)) return true;
	if (xy.test(EQ) && yz.test(BT)) return true;
	if (xy.test(BT) && true) return true;
	return false;
}

typedef bool(*FunctionPointer)(RelSet, RelSet);
typedef std::array<std::array<bool, 64>, 64> lookup_table;
static const lookup_table mk_lookup(FunctionPointer fun) {
	lookup_table result;
	for (std::size_t i = 0; i < 64; i++)
		for (std::size_t j = 0; j < 64; j++)
			result[i][j] = fun(RelSet(i), RelSet(j));
	return result;
}

static const std::array<lookup_table, 6> CONSISTENT_REL_LOOKUP {{
	mk_lookup(&consistentEQ),
	mk_lookup(&consistentMT),
	mk_lookup(&consistentMF),
	mk_lookup(&consistentGT),
	mk_lookup(&consistentGF),
	mk_lookup(&consistentBT)
}};

bool consistentRel(const Rel xz, const RelSet xy, const RelSet yz) {
	return CONSISTENT_REL_LOOKUP[xz][xy.to_ulong()][yz.to_ulong()];
}

bool tmr::consistent(const Shape& shape, std::size_t x, std::size_t z, Rel rel) {
	for (std::size_t y = 0; y < shape.size(); y++)
		if (!consistentRel(rel, shape.at(x,y), shape.at(y,z))) {
			// std::cout<<"InconsistentRel(x="<<x<<",y="<<y<<",z="<<z<<"): "<<x<<rel<<z<<" via "<<x<<shape.at(x,y)<<y<<" and "<<y<<shape.at(y,z)<<z<<std::endl;//<<" in shape "<<std::endl<<shape<<std::endl;
			return false;
		}
	return true;
}


/******************************** DISAMBIGUATION ********************************/

bool mk_concretisation(Shape& shape) {
	bool changed;
	do {
		changed = false;
		for (std::size_t i = 0; i < shape.size(); i++) {
			for (std::size_t j = i; j < shape.size(); j++) {
				for (Rel rel : shape.at(i, j)) {
					if (!consistent(shape, i, j, rel)) {
						shape.remove_relation(i, j, rel);
						changed = true;
					}
				}
				// if we removed all relations from one cell, then the shape
				// is no (partial) concretisation of the abstract input shape
				if (shape.at(i, j).none()) return false;
			}
		}
	} while (changed);
	return true;
}


Shape* tmr::isolate_partial_concretisation(const Shape& toSplit, const std::size_t row, const std::size_t col, const RelSet match) {
	assert(row < toSplit.size());
	assert(col < toSplit.size());
	RelSet new_cell = intersection(toSplit.at(row, col), match);
	
	if (new_cell.none()) return NULL;

	Shape* result = new Shape(toSplit);
	result->set(row, col, new_cell);
	bool success = mk_concretisation(*result);
	if (!success) {
		delete result;
		return NULL;
	}
	return result;
}


bool needsSplitting(RelSet rs) {
	auto count = rs.count();
	assert(count > 0);
	if (count == 1) return false;
	else if (count == 2) return rs != MT_GT && rs != MF_GF;
	else return true;
}

std::vector<RelSet> split_cell(RelSet rs) {
	std::vector<RelSet> result;
	result.reserve(4);
	if (rs.test(EQ)) result.push_back(EQ_);
	if (haveCommon(rs, MT_GT)) result.push_back(intersection(rs, MT_GT));
	if (haveCommon(rs, MF_GF)) result.push_back(intersection(rs, MF_GF));
	if (rs.test(BT)) result.push_back(BT_);
	assert(result.size() > 0);
	return result;
}

struct shape_ptr_comparator {
	bool operator()(const Shape* lhs, const Shape* rhs) {
		return *lhs < *rhs;
	}
};

std::vector<Shape*> tmr::disambiguate(const Shape& toSplit, const std::size_t row) {
	std::vector<Shape*> result;
	std::stack<std::pair<std::size_t, Shape*>> work;
	work.push(std::make_pair(0, new Shape(toSplit)));

	while (work.size() > 0) {
		std::size_t col = work.top().first;
		Shape* shape = work.top().second;
		if (col >= shape->size()) {
			// the shape is fully split up, so do one concretisation step
			bool success = mk_concretisation(*shape);
			if (success) {
				result.push_back(shape);
			} else {
				delete shape;
			}
			work.pop();
		} else if (col == row) {
			// disambiguate reflexivity
			work.top().first++;
		} else if (!needsSplitting(shape->at(row, col))) {
			// the cell (row, col) doesn't need disambiguation, so advance to next cell
			work.top().first++;
		} else {
			// remove inconsistent relations to avoid unnecessary work
			for (Rel rel : shape->at(row, col))
				if (!consistent(*shape, row, col, rel))
					shape->remove_relation(row, col, rel);

			if (shape->at(row, col).none()) {
				// we ran into an dead end, the shape will never be a concretization
				shape->set(row, col, BT_); // TODO: can this become a consistent concretization ????
				delete shape;
				work.pop();
				continue;
			}

			// now split if necessary
			work.top().first++;
			if (needsSplitting(shape->at(row, col))) {
				// split cell
				std::vector<RelSet> cellsplit = split_cell(shape->at(row, col));
				assert(cellsplit.size() > 1);
				// remember and pop last element - we will update it in-place to avoid one copy
				RelSet back = cellsplit.back();
				cellsplit.pop_back();
				// create new shapes
				for (RelSet rs : cellsplit) {
					Shape* s = new Shape(*shape);
					s->set(row, col, rs);
					work.push({ col+1, s });
				}
				// now do the in-place update
				// (do not do it earlier since we wanted to copy the unchanged shape)
				shape->set(row, col, back);
			}
		}
	}

	assert(result.size() > 0);
	return result;
}


/******************************** MERGE ********************************/

Shape* tmr::merge(std::vector<Shape*>& shapes) {
	if (shapes.size() == 0) return NULL;
	Shape* result = shapes.back();
	shapes.pop_back();
	for (std::size_t row = 0; row < result->size(); row++)
		for (std::size_t col = row; col < result->size(); col++) {
			RelSet mrg = result->at(row, col);
			for (const Shape* s : shapes)
				mrg = setunion(mrg, s->at(row, col));
			result->set(row, col, mrg);
		}
	for (Shape* rms : shapes) delete rms;
	shapes.clear();
	return result;
}


/******************************** GET/SET RELATED ********************************/

std::vector<std::size_t> tmr::get_related(const Shape& shape, std::size_t x, RelSet anyOf) {
	std::vector<std::size_t> result;
	result.reserve(shape.size());
	for (std::size_t i = 0; i < shape.size(); i++)
		if (haveCommon(shape.at(x, i), anyOf))
			result.push_back(i);
	return result;
}

void tmr::relate_all(Shape& shape, const std::vector<std::size_t>& vec1, const std::vector<std::size_t>& vec2, Rel rel) {
	for (std::size_t u : vec1)
		for (std::size_t v : vec2) {
			shape.remove_relation(u, v, BT);
			shape.add_relation(u, v, rel);
		}
}

void tmr::extend_all(Shape& shape, const std::vector<std::size_t>& vec1, const std::vector<std::size_t>& vec2, Rel rel) {
	for (std::size_t u : vec1)
		for (std::size_t v : vec2)
			shape.add_relation(u, v, rel);
}


/******************************** REMOVE SUCCESSORS ********************************/

void tmr::remove_successors(Shape& shape, const std::size_t x) {
	auto suc_x = get_related(shape, x, MT_GT);
	auto pre_x = get_related(shape, x, EQ_MF_GF);
	for (const auto u : suc_x)
		for (const auto v : pre_x) {
			assert(u != v);
			// assert( shape.at(v,u) ⊆ {↦,⇢} )
			assert(subset(shape.at(v, u), MT_GT));
			shape.set(v, u, BT);
		}
}
