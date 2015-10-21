#include "cfg.hpp"

using namespace tmr;


/************************ AGEREL ************************/

AgeRel::AgeRel(Type type) : _val(type) {
}

AgeRel::operator Type() const {
	return static_cast<Type>(_val);
}

AgeRel::Type AgeRel::type() const {
	return *this;
}

AgeRel AgeRel::symmetric() const {
	switch (type()) {
		case EQ: return EQ;
		case LT: return GT;
		case GT: return LT;
		case BOT: return BOT;
	}
}

std::ostream& tmr::operator<<(std::ostream& os, const AgeRel& r) {
	switch (r.type()) {
		case AgeRel::EQ: os << "="; break;
		case AgeRel::LT: os << "<"; break;
		case AgeRel::GT: os << ">"; break;
		case AgeRel::BOT: os << "⊥"; break;
	}
	return os;
}

/************************ AGEMATRIX ************************/

inline std::size_t mk_index(std::size_t row, std::size_t col) {
	assert(row < col);
	std::size_t result = (col-1)*col/2 + row;
	return result;
}

AgeMatrix::AgeMatrix(std::size_t numNonLocals, std::size_t numLocals, std::size_t numThreads)
	:   _bounds(3 + numNonLocals + numLocals*numThreads),
	  _rels(mk_index(0, _bounds+numLocals /* prepare for one extension */), AgeRel(AgeRel::BOT)) {
	set(0, 1, AgeRel::GT);
}

AgeRel AgeMatrix::at(std::size_t row, std::size_t col) const {
	assert(row < _bounds);
	assert(col < _bounds);
	if (col < row) return _rels[mk_index(col, row)].symmetric();
	else if (col == row) return AgeRel::EQ;
	else return _rels[mk_index(row, col)];
}

void AgeMatrix::set(std::size_t row, std::size_t col, AgeRel rel) {
	assert(row < _bounds);
	assert(col < _bounds);
	if (col < row) _rels[mk_index(col, row)] = rel.symmetric();
	else if (col == row) assert(rel == AgeRel::EQ);
	else _rels[mk_index(row, col)] = rel;
}

void AgeMatrix::extend(std::size_t numLocals) {
	_bounds += numLocals;
}

void AgeMatrix::shrink(std::size_t numLocals) {
	_bounds -= numLocals;
}

bool AgeMatrix::operator==(const AgeMatrix& other) const {
	for (std::size_t i = 0; i < _bounds; i++)
		for (std::size_t j = i + 1; j < _bounds; j++) {
			auto index = mk_index(i, j);
			if (!(_rels[index] == other._rels[index]))
				return false;
		}
	return true;
}

std::ostream& tmr::operator<<(std::ostream& os, const AgeMatrix& m) {
	os << "      \t";
	for (std::size_t i = 0; i < m.size(); i++)
		os << i << "   \t ";
	os << std::endl;
	for (std::size_t row = 0; row < m.size(); row++) {
		os << row << ":   \t";
		for (std::size_t col = 0; col < m.size(); col++) {
			auto cell = m.at(row, col);
			os << cell;
			os << "\t ";
		}
		os << std::endl;
	}
	return os;
}


/************************ OWNERSHIP ************************/

Ownership::Ownership(std::size_t first_obs, std::size_t first_global, std::size_t first_local, std::size_t max_size)
: _first_obs(first_obs), _first_global(first_global), _first_local(first_local), _bits(max_size, false) {}

bool Ownership::is_owned(std::size_t pos) const {
	return _bits[pos];
}

void Ownership::publish(std::size_t pos) {
	_bits[pos] = false;
}
void Ownership::own(std::size_t pos) {
	if (pos >= _first_local || (pos >= _first_obs && pos < _first_global))
		_bits[pos] = true;
}

void Ownership::set_ownership(std::size_t pos, bool val) {
	if (val) own(pos);
	else publish(pos);
}

bool Ownership::operator<(const Ownership& other) const {
	return _bits < other._bits;
}

bool Ownership::operator>(const Ownership& other) const {
	return _bits > other._bits;
}

void Ownership::print(std::ostream& os) const {
	for (std::size_t i = _first_local; i < _bits.size(); i++)
		if (_bits[i])
			os << "{" << i << "}";
}


/************************ OWNERSHIP ************************/

StrongInvalidity::StrongInvalidity(std::size_t max_size) : _bits(max_size, false) {
}

bool StrongInvalidity::is_strongly_invalid(std::size_t pos) const {
	return _bits[pos];
}

bool StrongInvalidity::at(std::size_t pos) const {
	return _bits[pos];
}

void StrongInvalidity::set(std::size_t pos, bool val) {
	_bits[pos] = val;
}

std::vector<bool>::reference StrongInvalidity::operator[](std::size_t pos) {
	return _bits[pos];
}

std::vector<bool>::const_reference StrongInvalidity::operator[](std::size_t pos) const {
	return _bits[pos];
}

bool StrongInvalidity::operator<(const StrongInvalidity& other) const {
	return _bits < other._bits;
}

bool StrongInvalidity::operator>(const StrongInvalidity& other) const {
	return _bits > other._bits;
}

void StrongInvalidity::print(std::ostream& os) const {
	for (std::size_t i = 0; i < _bits.size(); i++)
		if (_bits[i])
			os << "{" << i << "}";
}


/************************ CFG ************************/

Cfg Cfg::copy() const {
	assert(shape);
	return Cfg(*this, new Shape(*shape));
}

std::ostream& tmr::operator<<(std::ostream& os, const Cfg& cfg) {
	os << "(pc0=";
	if (cfg.pc[0]) os << *cfg.pc[0];
	else os << "NULL";
	os << ", pc1=";
	if (cfg.pc[1]) os << *cfg.pc[1];
	else os << "NULL";
	os << ", pc2=";
	if (cfg.pc[2]) os << *cfg.pc[2];
	else os << "NULL";
	os << ", state=" << cfg.state;
	os << ", inout0=" << cfg.inout[0];
	os << ", inout1=" << cfg.inout[1];
	os << ", inout2=" << cfg.inout[2];
	os << ", seen[0]=" << cfg.seen[0];
	os << ", seen[1]=" << cfg.seen[1];
	os << ", own="; cfg.own.print(os);
	os << ", sin="; cfg.sin.print(os);
	os << ", oracle[0]=" << cfg.oracle[0];
	os << ", oracle[1]=" << cfg.oracle[1];
	os << ", oracle[2]=" << cfg.oracle[2];
	os << ", shape";
	if (cfg.shape) os << ") with shape=" << "..." /* std::endl << *cfg.shape */ << std::endl;
	else os << "=NULL)" << std::endl;
	return os;
}

