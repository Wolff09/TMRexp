#include "cfg.hpp"

using namespace tmr;


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
	os << ", arg=[" << cfg.arg[0] << ", " << cfg.arg[1] << ", " << cfg.arg[2] << "]";
	os << ", datasel=" << cfg.datasel;
	os << ", epochesel=" << cfg.epochesel;
	os << ", shape";
	if (cfg.shape) os << ") with shape=" << "..." /* std::endl << *cfg.shape */ << std::endl;
	else os << "=NULL)" << std::endl;
	return os;
}

std::ostream& tmr::operator<<(std::ostream& os, const EpochValue& val) {
	os << "<";
	switch (val) {
		case EpochValue::ZERO: os << "e0"; break;
		case EpochValue::ONE: os << "e1"; break;
		case EpochValue::TWO: os << "e2"; break;
	}
	os << ">";
	return os;
}

template<typename T, T D>
void SelectorStore<T,D>::print(std::ostream& os) const {
	os << "{ ";
	if (_values.size() > 0) {
		os << _values[0];
		for (std::size_t i = 1; i < _values.size(); i++)
			os << ", " << _values[i];
	}
	os << " }";
}
