#include "helpers.hpp"

#include <set>
#include <stack>

using namespace tmr;


/******************************** SHAPE ASSERTIONS ********************************/

bool check_special_relations_constraints(const Shape& shape) {
	auto null = shape.index_NULL();
	auto rec = shape.index_REC();
	auto undef = shape.index_UNDEF();
	// ensure that null and undef are unrelated
	if (shape.at(null, undef).count() != 1 || !shape.at(null, undef).test(BT)) return false;
	// ensure that null and rec are unrelated
	if (shape.at(null, rec).count() != 1 || !shape.at(null, rec).test(BT)) return false;
	// ensure that undef and rec are unrelated
	if (shape.at(undef, rec).count() != 1 || !shape.at(undef, rec).test(BT)) return false;
	for (std::size_t i = shape.offset_vars(); i < shape.size(); i++) {
		// cell term i may only relate with =, ↦, ⇢ or ⋈ to NULL
		if (shape.at(i, null).test(MF)) return false;
		if (shape.at(i, null).test(GF)) return false;
		// cell term i may only relate with ↦, ⇢ or ⋈ to UNDEFINED
		if (shape.at(i, undef).test(EQ)) return false;
		if (shape.at(i, undef).test(MF)) return false;
		if (shape.at(i, undef).test(GF)) return false;
	}
	return true;
}


/******************************** CONSISTENCY ********************************/

static inline bool consistentEQ(RelSet xy, RelSet yz) {
	return (xy & symmetric(yz)).any();
}

static inline bool consistentMT(RelSet xy, RelSet yz) {
	if (xy.test(MT) && yz.test(EQ)) return true;
	if (xy.test(MF) && yz.test(GT)) return true;
	if (xy.test(GT) && (yz & MF_GF).any()) return true;
	if (xy.test(GF) && yz.test(GT)) return true;
	if (xy.test(EQ) && yz.test(MT)) return true;
	if (xy.test(BT) && (yz & MT_GT_BT).any()) return true;
	return false;
}

static inline bool consistentMF(RelSet xy, RelSet yz) {
	return consistentMT(symmetric(yz), symmetric(xy));
}

static inline bool consistentGT(RelSet xy, RelSet yz) {
	if (xy.test(MT) && (yz & MT_GT).any()) return true;
	if (xy.test(MF) && yz.test(GT)) return true;
	if (xy.test(GT) && (yz & EQ_MT_MF_GT_GF).any()) return true;
	if (xy.test(GF) && yz.test(GT)) return true;
	if (xy.test(EQ) && yz.test(GT)) return true;
	if (xy.test(BT) && (yz & MT_GT_BT).any()) return true;
	return false;
}

static inline bool consistentGF(RelSet xy, RelSet yz) {
	return consistentGT(symmetric(yz), symmetric(xy));
}

static inline bool consistentBT(RelSet xy, RelSet yz) {
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
static inline lookup_table mk_lookup(FunctionPointer fun) {
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

static bool consistentRel(const Rel xz, const RelSet xy, const RelSet yz) {
	return CONSISTENT_REL_LOOKUP[xz][xy.to_ulong()][yz.to_ulong()];
}

bool tmr::consistent(const Shape& shape, std::size_t x, std::size_t z, Rel rel) {
	for (std::size_t y = 0; y < shape.size(); y++)
		if (!consistentRel(rel, shape.at(x,y), shape.at(y,z))) {
			return false;
		}
	return true;
}

bool tmr::consistent(const Shape& shape) {
	assert(check_special_relations_constraints(shape));
	for (std::size_t x = 0; x < shape.size(); x++)
		if (shape.at(x, x) != EQ_)
			return false;
		else
			for (std::size_t z = x; z < shape.size(); z++)
				for (Rel rel : shape.at(x, z))
					if (!consistent(shape, x, z, rel))
						return false;
	return true;
}


/******************************** RELEXIVITY/TRANSITIVITY ********************************/

RelSet get_transitives(RelSet xy, RelSet yz) {
	RelSet result;
	if (xy.test(EQ)) result |= yz;
	if (yz.test(EQ)) result |= xy;
	if (xy.test(MT) && yz.test(MT)) result.set(GT);
	if (xy.test(MT) && yz.test(GT)) result.set(GT);
	if (xy.test(GT) && yz.test(MT)) result.set(GT);
	if (xy.test(GT) && yz.test(GT)) result.set(GT);
	if (xy.test(MF) && yz.test(MF)) result.set(GF);
	if (xy.test(MF) && yz.test(GF)) result.set(GF);
	if (xy.test(GF) && yz.test(MF)) result.set(GF);
	if (xy.test(GF) && yz.test(GF)) result.set(GF);
	return result;
}

bool tmr::is_closed_under_reflexivity_and_transitivity(const Shape& input, bool weak) {
	return true;
	Shape shape(input);
	bool updated;
	do {
		updated = false;
		for (std:: size_t x = 0; x < shape.size(); x++) {
			for (std:: size_t y = 0; y < shape.size(); y++) {
				for (std:: size_t z = 0; z < shape.size(); z++) {
					// if (x == z || x == y || y == z) continue;
					auto tc = get_transitives(shape.at(x, y), shape.at(y, z));
					auto su = setunion(shape.at(x, z), tc);
					updated |= (shape.at(x, z) != su);
					shape.set(x, z, su);
				}
			}
		}
	} while (updated);
	for (std:: size_t i = 0; i < shape.size(); i++)
		for (std:: size_t j = i+1; j < shape.size(); j++)
			if (shape.at(i, j) != input.at(i, j)) {
				if (weak)
					if (shape.test(i, i, GT) || shape.test(j, j, GT))
						// selfloop => transitivity got confused (non-trivial constraints not present in shape)
						return subset(input.at(i, j), shape.at(i, j));
				return false;
			}
	return true;
}


/******************************** DISAMBIGUATION ********************************/

bool is_concretisation(const Shape& con, const Shape& abs) {
	for (std::size_t i = 0; i < con.size(); i++)
		for (std::size_t j = 0; j < con.size(); j++)
			if (!subset(con.at(i, j), abs.at(i, j)) || con.at(i, j).none())
				return false;
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////

struct ConcretisationInfo {
	RelSet ij;
	RelSet ik;
	RelSet jk;
	bool changed;
	bool changed_ij;
	bool changed_ik;
	bool changed_jk;
	bool inconsistent;
	ConcretisationInfo() {}
	ConcretisationInfo(RelSet a, RelSet b, RelSet c) : ij(a), ik(b), jk(c) {}
};

static const ConcretisationInfo mk_cinfo(RelSet ij, RelSet ik, RelSet jk) {
	ConcretisationInfo result(ij, ik, jk);
	bool iterate;
	do {
		iterate = false;
		RelSet ij = result.ij;
		RelSet ik = result.ik;
		RelSet jk = result.jk;
		RelSet ji = symmetric(ij);
		RelSet kj = symmetric(jk);
		// Signature: consistentRel(const Rel xz, const RelSet xy, const RelSet yz)
		for (Rel rel : ij) { if (!consistentRel(rel, ik, kj)) { result.ij.reset(rel); iterate = true; goto ctn; }}
		for (Rel rel : ik) { if (!consistentRel(rel, ij, jk)) { result.ik.reset(rel); iterate = true; goto ctn; }}
		for (Rel rel : jk) { if (!consistentRel(rel, ji, ik)) { result.jk.reset(rel); iterate = true; goto ctn; }}
		ctn:;
	} while (iterate);
	result.changed_ij = ij != result.ij;
	result.changed_ik = ik != result.ik;
	result.changed_jk = jk != result.jk;
	result.changed = ij != result.ij || ik != result.ik || jk != result.jk;
	result.changed = result.changed_ij || result.changed_ik || result.changed_jk;
	result.inconsistent = result.ij.none() || result.ik.none() || result.jk.none();
	return result;
}

typedef std::array<std::array<std::array<ConcretisationInfo, 64>, 64>, 64> CONCRETISATION_LOOKUP_T;

static CONCRETISATION_LOOKUP_T mk_clookup() {
	CONCRETISATION_LOOKUP_T result;
	for (std::size_t i = 0; i < 64; i++)
		for (std::size_t j = 0; j < 64; j++)
			for (std::size_t k = 0; k < 64; k++)
				result[i][j][k] = mk_cinfo(RelSet(i), RelSet(j), RelSet(k));
	return result;
}

bool tmr::make_concretisation(Shape& shape) {
	static const CONCRETISATION_LOOKUP_T CONCRETISATION_LOOKUP = mk_clookup(); // likely ~10MB in size
	auto size = shape.size();
	bool changed;
	do {
		changed = false;
		for (std::size_t i = 0; i < size; i++) {
			for (std::size_t j = i; j < size; j++) {
				for (std::size_t k=j; k < size; k++) {
					auto cell_ij = shape.at(i, j).to_ulong();
					auto cell_ik = shape.at(i, k).to_ulong();
					auto cell_jk = shape.at(j, k).to_ulong();
					auto update = CONCRETISATION_LOOKUP[cell_ij][cell_ik][cell_jk];
					// TODO: could reading inconsistency from another table speed up things?
					if (update.inconsistent) return false;
					#if REPEAT_PRUNING
						changed |= update.changed;
					#endif
					// conditional update: update rarely necessary and "expensive" due to heavy load
					// also: branch prediction is our friend
					if (update.changed_ij) shape.set(i, j, update.ij);
					if (update.changed_ik) shape.set(i, k, update.ik);
					if (update.changed_jk) shape.set(j, k, update.jk);
				}
			}
		}
	} while (changed);
	return true;
}

////////////////////////////////////////////////////////////////////////////////////////

// bool tmr::make_concretisation(Shape& shape) {
// 	bool changed;
// 	do {
// 		changed = false;
// 		for (std::size_t i = 0; i < shape.size(); i++) {
// 			for (std::size_t j = i; j < shape.size(); j++) {
// 				for (Rel rel : shape.at(i, j)) {
// 					if (!consistent(shape, i, j, rel)) {
// 						shape.remove_relation(i, j, rel);
// 						changed = true;
// 					}
// 				}
// 				// if we removed all relations from one cell, then the shape
// 				// is no (partial) concretisation of the abstract input shape
// 				if (shape.at(i, j).none()) return false;
// 			}
// 		}
// 	} while (changed);
// 	return true;
// }


Shape* tmr::isolate_partial_concretisation(const Shape& toSplit, const std::size_t row, const std::size_t col, const RelSet match) {
	RelSet new_cell = intersection(toSplit.at(row, col), match);
	if (new_cell.none()) return NULL;

	Shape* result = new Shape(toSplit);
	result->set(row, col, new_cell);
	bool success = make_concretisation(*result);
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
	assert(consistent(toSplit));
	
	std::vector<Shape*> result;
	std::stack<std::pair<std::size_t, Shape*>> work;
	work.push(std::make_pair(0, new Shape(toSplit)));

	while (work.size() > 0) {
		std::size_t col = work.top().first;
		Shape* shape = work.top().second;
		if (col >= shape->size()) {
			// the shape is fully split up, so do one concretisation step
			bool success = make_concretisation(*shape);
			if (success) {
				assert(consistent(*shape));
				assert(is_concretisation(*shape, toSplit));
				assert(is_closed_under_reflexivity_and_transitivity(*shape));
				result.push_back(shape);
			} else {
				assert(!is_concretisation(*shape, toSplit));
				delete shape;
			}
			work.pop();
		} else if (col == row) {
			// disambiguate reflexivity
			assert(shape->at(col, row) == EQ_);
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
				delete shape;
				work.pop();
				continue;
			}

			// now split if necessary
			assert(is_concretisation(*shape, toSplit));
			work.top().first++;
			assert(work.top().first == col + 1);
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

std::vector<Shape*> tmr::disambiguate_cell(const Shape& shape, const std::size_t row, const std::size_t col) {
	if (!needsSplitting(shape.at(row, col))) return { new Shape(shape) };
	std::vector<RelSet> cellsplit = split_cell(shape.at(row, col));
	std::vector<Shape*> result;
	result.reserve(4);
	for (RelSet rs : cellsplit) {
		Shape* s = new Shape(shape);
		s->set(row, col, rs);
		bool success = make_concretisation(*s);
		if (success) result.push_back(s);
		else delete s;
	}
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
	assert(consistent(shape));
	auto suc_x = get_related(shape, x, MT_GT);
	auto pre_x = get_related(shape, x, EQ_MF_GF);
	for (const auto u : suc_x)
		for (const auto v : pre_x) {
			assert(u != v);
			// assert( shape.at(v,u) ⊆ {↦,⇢} )
			assert(subset(shape.at(v, u), MT_GT));
			shape.set(v, u, BT);
		}
	assert(consistent(shape));
}
