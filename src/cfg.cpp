#include "cfg.hpp"

using namespace tmr;


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
	os << ", state=" << cfg.state;
	os << ", inout0=" << cfg.inout[0];
	os << ", inout1=" << cfg.inout[1];
	os << ", inout2=" << cfg.inout[2];
	os << ", seen[0]=" << cfg.seen[0];
	os << ", seen[1]=" << cfg.seen[1];
	os << ", own="; cfg.own.print(os);
	os << ", oracle[0]=" << cfg.oracle[0];
	os << ", oracle[1]=" << cfg.oracle[1];
	os << ", oracle[2]=" << cfg.oracle[2];
	os << ", shape";
	if (cfg.shape) os << ") with shape=" << "..." /* std::endl << *cfg.shape */ << std::endl;
	else os << "=NULL)" << std::endl;
	return os;
}

