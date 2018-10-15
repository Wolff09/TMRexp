#pragma once

#include <stdexcept>
#include <memory>
#include <assert.h>
#include "prog.hpp"
#include "observer.hpp"
#include "config.hpp"

namespace tmr {

	inline const Function& find(const Program& prog, std::string name) {
		for (std::size_t i = 0; i < prog.size(); i++) {
			if (prog.at(i).name() == name) {
				return prog.at(i);
			}
		}
		throw std::logic_error("Invalid function name lookup.");
	}

	inline std::unique_ptr<State> mk_state(std::string name, bool initial, bool final) {
		return std::make_unique<State>(name, initial, final);
	}

	inline void mk_transition(State& src, const State& dst, Event trigger) {
		src.add_transition(std::make_unique<Transition>(trigger, dst));
	}


	static std::unique_ptr<Observer> ebr_observer(const Program& prog) {
		// get functions to react on
		const auto& f_enterQ = find(prog, "enterQ");
		const auto& f_leaveQ = find(prog, "leaveQ");
		const auto& f_retire = find(prog, "retire");
		
		// state names
		std::string n_freed       = "base:freed";
		std::string n_retired     = "base:retired";
		std::string n_dupfree     = "base:double-free";
		std::string n_dupretire   = "base:double-retire";
		std::string n_init        = "ebr:init";
		std::string n_leavingQ    = "ebr:leavingQ";
		std::string n_leftQ       = "ebr:leftQ";
		std::string n_leftretired = "ebr:retired";
		std::string n_final       = "ebr:freedprotected";

		// state vector
		std::vector<std::unique_ptr<State>> states;

		// base observer states
		states.push_back(mk_state(n_freed, true, false));
		states.push_back(mk_state(n_retired, false, false));
		states.push_back(mk_state(n_dupfree, false, true));
		states.push_back(mk_state(n_dupretire, false, false));

		// hp0 observer states
		states.push_back(mk_state(n_init, true, false));
		states.push_back(mk_state(n_leavingQ, false, false));
		states.push_back(mk_state(n_leftQ, false, false));
		states.push_back(mk_state(n_leftretired, false, false));
		states.push_back(mk_state(n_final, false, true));

		// get hold of the stats
		State& freed       = *states[0];
		State& retired     = *states[1];
		State& dupfree     = *states[2];
		State& dupretire   = *states[3];
		State& init        = *states[4];
		State& leavingQ    = *states[5];
		State& leftQ       = *states[6];
		State& leftretired = *states[7];
		State& final       = *states[8];

		// shortcuts
		auto ADR = DataValue::DATA;
		auto OTHER = DataValue::OTHER;

		// base observer transitions
		mk_transition(freed, retired, Event::mk_enter(f_retire, 0, ADR));
		mk_transition(freed, retired, Event::mk_enter(f_retire, 1, ADR));
		mk_transition(freed, retired, Event::mk_enter(f_retire, 2, ADR));
		mk_transition(retired, dupretire, Event::mk_enter(f_retire, 0, ADR));
		mk_transition(retired, dupretire, Event::mk_enter(f_retire, 1, ADR));
		mk_transition(retired, dupretire, Event::mk_enter(f_retire, 2, ADR));
		mk_transition(retired, freed, Event::mk_free(ADR));
		mk_transition(freed, dupfree, Event::mk_free(ADR));

		// hp0 observer transitions
		mk_transition(init, leavingQ, Event::mk_enter(f_leaveQ, 0, ADR));
		mk_transition(init, leavingQ, Event::mk_enter(f_leaveQ, 0, OTHER));
		mk_transition(leavingQ, leftQ, Event::mk_exit(0));
		mk_transition(leftQ, leftretired, Event::mk_enter(f_retire, 0, ADR));
		mk_transition(leftQ, leftretired, Event::mk_enter(f_retire, 1, ADR));
		mk_transition(leftQ, leftretired, Event::mk_enter(f_retire, 2, ADR));
		mk_transition(leftretired, final, Event::mk_free(ADR));
		mk_transition(leavingQ, init, Event::mk_enter(f_enterQ, 0, ADR));
		mk_transition(leavingQ, init, Event::mk_enter(f_enterQ, 0, OTHER));
		mk_transition(leftQ, init, Event::mk_enter(f_enterQ, 0, ADR));
		mk_transition(leftQ, init, Event::mk_enter(f_enterQ, 0, OTHER));
		mk_transition(leftretired, init, Event::mk_enter(f_enterQ, 0, ADR));
		mk_transition(leftretired, init, Event::mk_enter(f_enterQ, 0, OTHER));

		// done
		return std::make_unique<Observer>(std::move(states));
	}


	static std::unique_ptr<Observer> hp_observer(const Program& prog) {
		// get functions to react on
		const auto& f_protect0 = find(prog, "protect0");
		const auto& f_protect1 = find(prog, "protect1");
		const auto& f_unprotect0 = find(prog, "unprotect0");
		const auto& f_unprotect1 = find(prog, "unprotect1");
		const auto& f_retire = find(prog, "retire");
		
		// state names
		std::string n_freed             = "base:freed";
		std::string n_retired           = "base:retired";
		std::string n_dupfree           = "base:double-free";
		std::string n_dupretire         = "base:double-retire";
		std::string n_init0             = "hp0:init";
		std::string n_entered0          = "hp0:entered";
		std::string n_protected0        = "hp0:exited";
		std::string n_protectedretired0 = "hp0:retired";
		std::string n_final0            = "hp0:freedprotected";
		std::string n_init1             = "hp1:init";
		std::string n_entered1          = "hp1:entered";
		std::string n_protected1        = "hp1:exited";
		std::string n_protectedretired1 = "hp1:retired";
		std::string n_final1            = "hp1:freedprotected";

		// state vector
		std::vector<std::unique_ptr<State>> states;

		// base observer states
		states.push_back(mk_state(n_freed, true, false));
		states.push_back(mk_state(n_retired, false, false));
		states.push_back(mk_state(n_dupfree, false, true));
		states.push_back(mk_state(n_dupretire, false, false));

		// hp0 observer states
		states.push_back(mk_state(n_init0, true, false));
		states.push_back(mk_state(n_entered0, false, false));
		states.push_back(mk_state(n_protected0, false, false));
		states.push_back(mk_state(n_protectedretired0, false, false));
		states.push_back(mk_state(n_final0, false, true));

		// hp1 observer states
		states.push_back(mk_state(n_init1, true, false));
		states.push_back(mk_state(n_entered1, false, false));
		states.push_back(mk_state(n_protected1, false, false));
		states.push_back(mk_state(n_protectedretired1, false, false));
		states.push_back(mk_state(n_final1, false, true));

		// get hold of the stats
		State& freed             = *states[0];
		State& retired           = *states[1];
		State& dupfree           = *states[2];
		State& dupretire         = *states[3];
		State& init0             = *states[4];
		State& entered0          = *states[5];
		State& exited0           = *states[6];
		State& protectedretired0 = *states[7];
		State& final0            = *states[8];
		State& init1             = *states[9];
		State& entered1          = *states[10];
		State& exited1           = *states[11];
		State& protectedretired1 = *states[12];
		State& final1            = *states[13];

		// shortcuts
		auto ADR = DataValue::DATA;
		auto OTHER = DataValue::OTHER;

		// base observer transitions
		mk_transition(freed, retired, Event::mk_enter(f_retire, 0, ADR));
		mk_transition(freed, retired, Event::mk_enter(f_retire, 1, ADR));
		mk_transition(freed, retired, Event::mk_enter(f_retire, 2, ADR));
		mk_transition(retired, dupretire, Event::mk_enter(f_retire, 0, ADR));
		mk_transition(retired, dupretire, Event::mk_enter(f_retire, 1, ADR));
		mk_transition(retired, dupretire, Event::mk_enter(f_retire, 2, ADR));
		mk_transition(retired, freed, Event::mk_free(ADR));
		mk_transition(freed, dupfree, Event::mk_free(ADR));

		// hp0 observer transitions
		mk_transition(init0, entered0, Event::mk_enter(f_protect0, 0, ADR));
		mk_transition(entered0, exited0, Event::mk_exit(0));
		mk_transition(exited0, protectedretired0, Event::mk_enter(f_retire, 0, ADR));
		mk_transition(exited0, protectedretired0, Event::mk_enter(f_retire, 1, ADR));
		mk_transition(exited0, protectedretired0, Event::mk_enter(f_retire, 2, ADR));
		mk_transition(protectedretired0, final0, Event::mk_free(ADR));
		mk_transition(entered0, init0, Event::mk_enter(f_protect0, 0, OTHER));
		mk_transition(entered0, init0, Event::mk_enter(f_unprotect0, 0, ADR));
		mk_transition(entered0, init0, Event::mk_enter(f_unprotect0, 0, OTHER));
		mk_transition(exited0, init0, Event::mk_enter(f_protect0, 0, OTHER));
		mk_transition(exited0, init0, Event::mk_enter(f_unprotect0, 0, ADR));
		mk_transition(exited0, init0, Event::mk_enter(f_unprotect0, 0, OTHER));
		mk_transition(protectedretired0, init0, Event::mk_enter(f_protect0, 0, OTHER));
		mk_transition(protectedretired0, init0, Event::mk_enter(f_unprotect0, 0, ADR));
		mk_transition(protectedretired0, init0, Event::mk_enter(f_unprotect0, 0, OTHER));

		// hp1 observer transitions
		mk_transition(init1, entered1, Event::mk_enter(f_protect1, 0, ADR));
		mk_transition(entered1, exited1, Event::mk_exit(0));
		mk_transition(exited1, protectedretired1, Event::mk_enter(f_retire, 0, ADR));
		mk_transition(exited1, protectedretired1, Event::mk_enter(f_retire, 1, ADR));
		mk_transition(exited1, protectedretired1, Event::mk_enter(f_retire, 2, ADR));
		mk_transition(protectedretired1, final1, Event::mk_free(ADR));
		mk_transition(entered1, init1, Event::mk_enter(f_protect1, 0, OTHER));
		mk_transition(entered1, init1, Event::mk_enter(f_unprotect1, 0, ADR));
		mk_transition(entered1, init1, Event::mk_enter(f_unprotect1, 0, OTHER));
		mk_transition(exited1, init1, Event::mk_enter(f_protect1, 0, OTHER));
		mk_transition(exited1, init1, Event::mk_enter(f_unprotect1, 0, ADR));
		mk_transition(exited1, init1, Event::mk_enter(f_unprotect1, 0, OTHER));
		mk_transition(protectedretired1, init1, Event::mk_enter(f_protect1, 0, OTHER));
		mk_transition(protectedretired1, init1, Event::mk_enter(f_unprotect1, 0, ADR));
		mk_transition(protectedretired1, init1, Event::mk_enter(f_unprotect1, 0, OTHER));

		// done
		return std::make_unique<Observer>(std::move(states));
	}

}