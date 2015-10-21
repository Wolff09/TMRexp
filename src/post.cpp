#include "post.hpp"

#include <assert.h>
#include "helpers.hpp"

using namespace tmr;


std::ostream& tmr::operator<<(std::ostream& os, const MemorySetup& msetup) {
	switch (msetup) {
		case GC: return os << "GarbageCollection";
		case PRF: return os << "PointerRaceFreedom";
		case MM: return os << "MemmoryManaged";
		default: assert(false);
	}
}


std::vector<Cfg> tmr::post(const Cfg& cfg, unsigned short tid, MemorySetup msetup) {
	assert(cfg.pc[tid] != NULL);
	assert(cfg.pc[tid] != NULL);
	assert(cfg.shape != NULL);
	assert(consistent(*cfg.shape));
	const Statement& stmt = *cfg.pc[tid];
	// std::cout << "$$$$$$$$$$$$$$$$$$$$$$$ post for tid="<<tid<<": " << cfg << *cfg.shape << std::endl;
	// std::cout << "    shape: " << std::endl << *cfg.shape << std::endl;
	switch (stmt.clazz()) {
		case Statement::SQZ:     return tmr::post(cfg, static_cast<const              Sequence&>(stmt), tid, msetup);
		case Statement::ATOMIC:  return tmr::post(cfg, static_cast<const                Atomic&>(stmt), tid, msetup);
		case Statement::CAS:     return tmr::post(cfg, static_cast<const        CompareAndSwap&>(stmt), tid, msetup);
		case Statement::ASSIGN:  return tmr::post(cfg, static_cast<const            Assignment&>(stmt), tid, msetup);
		case Statement::SETNULL: return tmr::post(cfg, static_cast<const        NullAssignment&>(stmt), tid, msetup);
		case Statement::INPUT:   return tmr::post(cfg, static_cast<const   ReadInputAssignment&>(stmt), tid, msetup);
		case Statement::OUTPUT:  return tmr::post(cfg, static_cast<const WriteOutputAssignment&>(stmt), tid, msetup);
		case Statement::MALLOC:  return tmr::post(cfg, static_cast<const                Malloc&>(stmt), tid, msetup);
		case Statement::FREE:    return tmr::post(cfg, static_cast<const                  Free&>(stmt), tid, msetup);
		case Statement::BREAK:   return tmr::post(cfg, static_cast<const                 Break&>(stmt), tid, msetup);
		case Statement::LINP:    return tmr::post(cfg, static_cast<const    LinearizationPoint&>(stmt), tid, msetup);
		case Statement::ITE:     return tmr::post(cfg, static_cast<const                   Ite&>(stmt), tid, msetup);
		case Statement::WHILE:   return tmr::post(cfg, static_cast<const                 While&>(stmt), tid, msetup);
		case Statement::ORACLE:  return tmr::post(cfg, static_cast<const                Oracle&>(stmt), tid, msetup);
		case Statement::CHECKP:  return tmr::post(cfg, static_cast<const         CheckProphecy&>(stmt), tid, msetup);
		case Statement::KILL:    return tmr::post(cfg, static_cast<const                Killer&>(stmt), tid, msetup);
	}
	assert(false);
}
