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
	os << ", offender=[" << cfg.offender[0] << ", " << cfg.offender[1] << ", " << cfg.offender[2] << "]";
	os << ", datasel0=" << cfg.datasel0;
	os << ", datasel1=" << cfg.datasel1;
	os << ", dataset0=[" << cfg.dataset0[0] << ", " << cfg.dataset0[1] << ", " << cfg.dataset0[2] << "]";
	os << ", dataset1=[" << cfg.dataset1[0] << ", " << cfg.dataset1[1] << ", " << cfg.dataset1[2] << "]";
	os << ", dataset2=[" << cfg.dataset2[0] << ", " << cfg.dataset2[1] << ", " << cfg.dataset2[2] << "]";
	os << ", shape";
	if (cfg.shape) os << ") with shape=" << "..." /* std::endl << *cfg.shape */ << std::endl;
	else os << "=NULL)" << std::endl;
	return os;
}

std::ostream& tmr::operator<<(std::ostream& os, const DataSet& val) {
	os << "<";
	switch (val) {
		case DataSet::WITH_DATA: os << "{D,?}"; break;
		case DataSet::WITHOUT_DATA: os << "{?}"; break;
	}
	os << ">";
	return os;
}
