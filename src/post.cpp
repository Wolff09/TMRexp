#include "post.hpp"

#include <assert.h>
#include <deque>
#include "helpers.hpp"
#include "config.hpp"

using namespace tmr;


static std::vector<Cfg> get_post_cfgs(const Cfg& cfg, unsigned short tid) {
	assert(cfg.pc[tid] != NULL);
	assert(cfg.pc[tid] != NULL);
	assert(cfg.shape != NULL);
	assert(consistent(*cfg.shape));
	const Statement& stmt = *cfg.pc[tid];
	switch (stmt.clazz()) {
		case Statement::SQZ:     return tmr::post(cfg, static_cast<const              Sequence&>(stmt), tid);
		case Statement::ATOMIC:  return tmr::post(cfg, static_cast<const                Atomic&>(stmt), tid);
		case Statement::CAS:     return tmr::post(cfg, static_cast<const        CompareAndSwap&>(stmt), tid);
		case Statement::ASSIGN:  return tmr::post(cfg, static_cast<const            Assignment&>(stmt), tid);
		case Statement::SETNULL: return tmr::post(cfg, static_cast<const        NullAssignment&>(stmt), tid);
		case Statement::INPUT:   return tmr::post(cfg, static_cast<const   ReadInputAssignment&>(stmt), tid);
		case Statement::OUTPUT:  return tmr::post(cfg, static_cast<const WriteOutputAssignment&>(stmt), tid);
		case Statement::MALLOC:  return tmr::post(cfg, static_cast<const                Malloc&>(stmt), tid);
		case Statement::FREE:    return tmr::post(cfg, static_cast<const                  Free&>(stmt), tid);
		case Statement::BREAK:   return tmr::post(cfg, static_cast<const                 Break&>(stmt), tid);
		case Statement::LINP:    return tmr::post(cfg, static_cast<const    LinearizationPoint&>(stmt), tid);
		case Statement::ITE:     return tmr::post(cfg, static_cast<const                   Ite&>(stmt), tid);
		case Statement::WHILE:   return tmr::post(cfg, static_cast<const                 While&>(stmt), tid);
		case Statement::ORACLE:  return tmr::post(cfg, static_cast<const                Oracle&>(stmt), tid);
		case Statement::CHECKP:  return tmr::post(cfg, static_cast<const         CheckProphecy&>(stmt), tid);
		case Statement::KILL:    return tmr::post(cfg, static_cast<const                Killer&>(stmt), tid);
		case Statement::REACH:   return tmr::post(cfg, static_cast<const          EnforceReach&>(stmt), tid);
	}
	assert(false);
}


std::vector<Cfg> tmr::post(const Cfg& cfg, unsigned short tid) {
	auto post = get_post_cfgs(cfg, tid);

	#if HINTING
		const Program* prog = NULL;
		if (cfg.pc[0]) prog = &cfg.pc[0]->function().prog();
		if (cfg.pc[1]) prog = &cfg.pc[1]->function().prog();
		if (prog && prog->has_hint()) {
			std::vector<Cfg> cppost;
			cppost.reserve(post.size());
			for (auto& c : post) {
				bool rm = prog->apply_hint(c.shape.get());
				if (!rm) {
					cppost.push_back(std::move(c));
				}
			}
			post = std::move(cppost);
		}
	#endif

	return post;
}
