#include "shape.hpp"

using namespace tmr;


/******************************** CONSTRUCTION ********************************/

Shape::Shape(std::size_t numObsVars, std::size_t numGlobVars, std::size_t numLocVars, unsigned short numThreads) :
             _numGlobVars(numGlobVars),
             _numLocVars(numLocVars),
             _bounds(3 + 2 + numGlobVars + numThreads*numLocVars) {

	// init shape
	RelSet dummy_cell = singleton(BT);
	std::vector<RelSet> dummy_row(_bounds + _numLocVars, dummy_cell);
	_cells = std::vector<std::vector<RelSet>>(_bounds + _numLocVars, dummy_row);
	for (std::size_t i = offset_vars(); i < _cells.size(); i++) set(i, index_UNDEF(), MT);
	set(index_REUSE(), index_NULL(), MT);
	for (std::size_t i = 0; i < _cells.size(); i++) _cells[i][i] = singleton(EQ);

	if (numObsVars != _numObsVars) throw std::logic_error("Number of observer variables not supported.");
	if (numThreads != 1) throw std::logic_error("There must be exactly one threads.");
}


/******************************** EXTENSION/SHRINKING ********************************/

void Shape::extend() {
	_bounds += _numLocVars;
}

void Shape::shrink() {
	_bounds -= _numLocVars;
}


/******************************** ACCESS ********************************/

RelSet Shape::at(std::size_t i, std::size_t j) const {
	// assert(i < _bounds && j < _bounds);
	return _cells[i][j];
}

bool Shape::test(std::size_t i, std::size_t j, Rel r) const {
	// assert(i < _bounds && j < _bounds);
	return _cells[i][j].test(r);
}


/******************************** MODIFICATION ********************************/

static inline std::array<RelSet, 64> mk_lookup() {
	std::array<RelSet, 64> result;
	for (std::size_t k = 0; k < 64; k++)
		result[k] = symmetric(RelSet(k));
	return result;
}

void Shape::set(std::size_t i, std::size_t j, RelSet rs) {
	static const std::array<RelSet, 64> SYMMETRIC_LOOKUP = mk_lookup();
	assert(i < _bounds && j < _bounds);
	_cells[i][j] = rs;
	_cells[j][i] = SYMMETRIC_LOOKUP[rs.to_ulong()]; // symmetric(rs);
	assert(_cells[i][j].any());
	assert(_cells[j][i].any());
}

void Shape::remove_relation(std::size_t i, std::size_t j, Rel r) {
	assert(i < _bounds && j < _bounds);
	_cells[i][j].set(r, false);
	_cells[j][i].set(symmetric(r), false);
}

void Shape::add_relation(std::size_t i, std::size_t j, Rel r) {
	assert(i < _bounds && j < _bounds);
	_cells[i][j].set(r, true);
	_cells[j][i].set(symmetric(r), true);
}


/******************************** PRINTING ********************************/

std::ostream& tmr::operator<<(std::ostream& os, const Shape& shape) {
	shape.print(os);
	return os;
}

void Shape::print(std::ostream& os) const {
	os << "      \t";
	for (std::size_t i = 0; i < size(); i++)
		os << i << "   \t ";
	os << std::endl;
	for (std::size_t row = 0; row < size(); row++) {
		os << row << ":   \t";
		for (std::size_t col = 0; col < size(); col++) {
			auto cell = _cells[row][col];
			if (contains(cell, EQ)) os << EQ;
			if (contains(cell, MT)) os << MT;
			if (contains(cell, MF)) os << MF;
			if (contains(cell, GT)) os << GT;
			if (contains(cell, GF)) os << GF;
			if (contains(cell, BT)) os << BT;
			if (cell.none()) os << "∅";
			os << "\t ";
		}
		os << std::endl;
	}
}


/******************************** COMPARISON ********************************/

bool Shape::operator<(const Shape& other) const {
	assert(size() == other.size());
	for (std::size_t i = 0; i < _cells.size(); i++) {
		for (std::size_t j = i+1; j < _cells.at(i).size(); j++) {
			auto l = _cells[i][j].to_ulong();
			auto r = other._cells[i][j].to_ulong();
			if (l < r) return true;
			if (l > r) return false;
		}
	}
	return false;
}

bool Shape::operator==(const Shape& other) const {
	assert(size() == other.size());
	for (std::size_t i = 0; i < _cells.size(); i++) {
		for (std::size_t j = i+1; j < _cells.at(i).size(); j++) {
			auto l = _cells[i][j].to_ulong();
			auto r = other._cells[i][j].to_ulong();
			if (l != r) return false;
		}
	}
	return true;
}
