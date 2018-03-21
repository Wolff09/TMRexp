#include "cfg.hpp"

using namespace tmr;


template<typename T, T D>
void DynamicStore<T, D>::print(std::ostream& os) const {
	os << "(+" << _offset << ")[";
	for (const auto& elem : _store)
		os << "" << elem << ", ";
	os << "]";
}

/************************ CFG ************************/

Cfg Cfg::copy() const {
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
	os << ", inout=[" << cfg.inout[0] << ", " << cfg.inout[1] << ", " << cfg.inout[2] << "]";
	os << ", seen[0]=[" << cfg.seen[0] << ", " << cfg.seen[1] << "]";
	os << ", own="; cfg.own.print(os);
	os << ", valid_ptr="; cfg.valid_ptr.print(os);
	os << ", valid_next="; cfg.valid_next.print(os);
	os << ", guard0="; cfg.guard0state.print(os);
	os << ", guard1="; cfg.guard1state.print(os);
	os << ", freed=" << cfg.freed;
	os << ", retired=" << cfg.retired;
	os << ", oracle[0]=[" << cfg.oracle[0] << ", " << cfg.oracle[1] << ", " << cfg.oracle[2] << "]";
	os << ", shape";
	if (cfg.shape) os << ") with shape=" << "..." /* std::endl << *cfg.shape */ << std::endl;
	else os << "=NULL)" << std::endl;
	return os;
}

