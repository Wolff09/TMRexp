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
	os << ", epoch=" << cfg.globalepoch;
	os << ", owned=[" << cfg.owned[0] << ", " << cfg.owned[1] << "]";
	os << ", smrstate=" << cfg.smrstate;
	os << ", threadstate=[" << cfg.threadstate[0] << ", " << cfg.threadstate[1] << "]";
	os << ", arg=[" << cfg.arg[0] << ", " << cfg.arg[1] << "]";
	os << ", offender=[" << cfg.offender[0] << ", " << cfg.offender[1] << "]";
	os << ", datasel0=" << cfg.datasel0;
	os << ", datasel1=" << cfg.datasel1;
	os << ", epochsel=" << cfg.epochsel;
	os << ", localepoch=[" << cfg.localepoch[0] << ", " << cfg.localepoch[1] << "]";
	os << ", dataset0=[" << cfg.dataset0[0] << ", " << cfg.dataset0[1] << "]";
	os << ", dataset1=[" << cfg.dataset1[0] << ", " << cfg.dataset1[1] << "]";
	os << ", dataset2=[" << cfg.dataset2[0] << ", " << cfg.dataset2[1] << "]";
	os << ", shape";
	if (cfg.shape) {
		os << ") with shape=";
		os << "...";
		// os << std::endl << *cfg.shape;
		os << std::endl;
	}
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

std::ostream& tmr::operator<<(std::ostream& os, const Epoch& epoch) {
	os << "<";
	switch (epoch) {
		case Epoch::ZERO: os << "<0>"; break;
		case Epoch::ONE: os << "<1>"; break;
		case Epoch::TWO: os << "<2>"; break;
	}
	os << ">";
	return os;
}
