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

	inline std::unique_ptr<State> mk_state(std::string name, bool initial=false, bool final=false, bool marked=false, std::size_t color=0) {
		if (color == 0) return std::make_unique<State>(name, initial, final, marked);
		else return std::make_unique<State>(name, initial, final, marked, color);
	}

	inline std::unique_ptr<State> mk_state_init(std::string name) {
		return mk_state(name, true, false, false);
	}

	inline std::unique_ptr<State> mk_state_final(std::string name) {
		return mk_state(name, false, true, false);
	}

	inline std::unique_ptr<State> mk_state_marked(std::string name) {
		return mk_state(name, false, false, true);
	}

	inline std::unique_ptr<State> mk_state_colored(std::string name) {
		return mk_state(name, false, false, false, 1);
	}

	inline void mk_transition(State& src, const State& dst, Event trigger) {
		src.add_transition(std::make_unique<Transition>(trigger, dst));
	}


	static std::unique_ptr<Observer> base_observer(const Program& prog) {
		// get functions to react on
		const auto& f_retire = find(prog, "retire");
		
		// state names
		std::string n_freed             = "base:freed";
		std::string n_retired0          = "base:retired0";
		std::string n_retired1          = "base:retired1";
		std::string n_dupfree           = "base:double-free";
		std::string n_dupretire         = "base:double-retire";

		// state vector
		std::vector<std::unique_ptr<State>> states;

		// base observer states
		states.push_back(mk_state_init(n_freed));
		states.push_back(mk_state(n_retired0));
		states.push_back(mk_state_colored(n_retired1));
		states.push_back(mk_state_final(n_dupfree));
		states.push_back(mk_state_marked(n_dupretire));

		// get hold of the stats
		State& freed             = *states[0];
		State& retired0          = *states[1];
		State& retired1          = *states[2];
		State& dupfree           = *states[3];
		State& dupretire         = *states[4];

		// shortcuts
		auto ADR = DataValue::DATA;

		// observer transitions
		mk_transition(freed, retired1, Event::mk_enter(f_retire, true, ADR));
		mk_transition(freed, retired0, Event::mk_enter(f_retire, false, ADR));
		mk_transition(retired0, dupretire, Event::mk_enter(f_retire, true, ADR));
		mk_transition(retired0, dupretire, Event::mk_enter(f_retire, false, ADR));
		mk_transition(retired1, dupretire, Event::mk_enter(f_retire, true, ADR));
		mk_transition(retired1, dupretire, Event::mk_enter(f_retire, false, ADR));
		mk_transition(retired0, freed, Event::mk_free(true, ADR));
		mk_transition(retired0, freed, Event::mk_free(false, ADR));
		mk_transition(retired1, freed, Event::mk_free(true, ADR));
		mk_transition(retired1, freed, Event::mk_free(false, ADR));
		mk_transition(freed, dupfree, Event::mk_free(true, ADR));

		// done
		return std::make_unique<Observer>(std::move(states));
	}


	static std::unique_ptr<Observer> base_observer_with_EBR_assumption(const Program& prog) {
		// get functions to react on
		const auto& f_retire = find(prog, "retire");
		const auto& f_enterQ = find(prog, "enterQ");
		const auto& f_leaveQ = find(prog, "leaveQ");
		
		// state names
		std::string n_freed     = "base:freed";
		std::string n_retired0  = "base:retired0";
		std::string n_retired1  = "base:retired1";
		std::string n_dupfree   = "base:double-free";
		std::string n_dupretire = "base:double-retire";
		std::string n_inQ       = "inv:inQ";
		std::string n_outQ      = "inv:outQ";
		std::string n_sink      = "inv:sink";

		// state vector
		std::vector<std::unique_ptr<State>> states;

		// base observer states
		states.push_back(mk_state_init(n_freed));
		states.push_back(mk_state(n_retired0));
		states.push_back(mk_state_colored(n_retired1));
		states.push_back(mk_state_final(n_dupfree));
		states.push_back(mk_state_marked(n_dupretire));

		// invariant observer states
		states.push_back(mk_state_init(n_inQ));
		states.push_back(mk_state(n_outQ));
		states.push_back(mk_state_marked(n_sink));

		// get hold of the stats
		State& freed     = *states[0];
		State& retired0  = *states[1];
		State& retired1  = *states[2];
		State& dupfree   = *states[3];
		State& dupretire = *states[4];
		State& inQ       = *states[5];
		State& outQ      = *states[6];
		State& sink      = *states[7];

		// shortcuts
		auto ADR = DataValue::DATA;
		auto OTHER = DataValue::OTHER;

		// observer transitions
		mk_transition(freed, retired1, Event::mk_enter(f_retire, true, ADR));
		mk_transition(freed, retired0, Event::mk_enter(f_retire, false, ADR));
		mk_transition(retired0, dupretire, Event::mk_enter(f_retire, true, ADR));
		mk_transition(retired0, dupretire, Event::mk_enter(f_retire, false, ADR));
		mk_transition(retired1, dupretire, Event::mk_enter(f_retire, true, ADR));
		mk_transition(retired1, dupretire, Event::mk_enter(f_retire, false, ADR));
		mk_transition(retired0, freed, Event::mk_free(true, ADR));
		mk_transition(retired0, freed, Event::mk_free(false, ADR));
		mk_transition(retired1, freed, Event::mk_free(true, ADR));
		mk_transition(retired1, freed, Event::mk_free(false, ADR));
		mk_transition(freed, dupfree, Event::mk_free(true, ADR));

		// ebr observer transitions
		mk_transition(inQ, outQ, Event::mk_enter(f_leaveQ, true, ADR));
		mk_transition(inQ, outQ, Event::mk_enter(f_leaveQ, true, OTHER));
		mk_transition(outQ, inQ, Event::mk_enter(f_enterQ, true, ADR));
		mk_transition(outQ, inQ, Event::mk_enter(f_enterQ, true, OTHER));
		mk_transition(outQ, sink, Event::mk_enter(f_leaveQ, true, ADR));
		mk_transition(outQ, sink, Event::mk_enter(f_leaveQ, true, OTHER));

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

		// hp0 observer states
		states.push_back(mk_state_init(n_init0));
		states.push_back(mk_state(n_entered0));
		states.push_back(mk_state(n_protected0));
		states.push_back(mk_state(n_protectedretired0));
		states.push_back(mk_state_final(n_final0));

		// hp1 observer states
		states.push_back(mk_state_init(n_init1));
		states.push_back(mk_state(n_entered1));
		states.push_back(mk_state(n_protected1));
		states.push_back(mk_state(n_protectedretired1));
		states.push_back(mk_state_final(n_final1));

		// get hold of the stats
		State& init0             = *states[0];
		State& entered0          = *states[1];
		State& exited0           = *states[2];
		State& protectedretired0 = *states[3];
		State& final0            = *states[4];
		State& init1             = *states[5];
		State& entered1          = *states[6];
		State& exited1           = *states[7];
		State& protectedretired1 = *states[8];
		State& final1            = *states[9];

		// shortcuts
		auto ADR = DataValue::DATA;
		auto OTHER = DataValue::OTHER;

		// hp0 observer transitions
		mk_transition(init0, entered0, Event::mk_enter(f_protect0, true, ADR));
		mk_transition(entered0, exited0, Event::mk_exit(true));
		mk_transition(exited0, protectedretired0, Event::mk_enter(f_retire, true, ADR));
		mk_transition(exited0, protectedretired0, Event::mk_enter(f_retire, false, ADR));
		mk_transition(protectedretired0, final0, Event::mk_free(true, ADR));
		mk_transition(protectedretired0, final0, Event::mk_free(false, ADR));
		mk_transition(entered0, init0, Event::mk_enter(f_protect0, true, OTHER));
		mk_transition(entered0, init0, Event::mk_enter(f_unprotect0, true, ADR));
		mk_transition(entered0, init0, Event::mk_enter(f_unprotect0, true, OTHER));
		mk_transition(exited0, init0, Event::mk_enter(f_protect0, true, OTHER));
		mk_transition(exited0, init0, Event::mk_enter(f_unprotect0, true, ADR));
		mk_transition(exited0, init0, Event::mk_enter(f_unprotect0, true, OTHER));
		mk_transition(protectedretired0, init0, Event::mk_enter(f_protect0, true, OTHER));
		mk_transition(protectedretired0, init0, Event::mk_enter(f_unprotect0, true, ADR));
		mk_transition(protectedretired0, init0, Event::mk_enter(f_unprotect0, true, OTHER));

		// hp1 observer transitions
		mk_transition(init1, entered1, Event::mk_enter(f_protect1, true, ADR));
		mk_transition(entered1, exited1, Event::mk_exit(true));
		mk_transition(exited1, protectedretired1, Event::mk_enter(f_retire, true, ADR));
		mk_transition(exited1, protectedretired1, Event::mk_enter(f_retire, false, ADR));
		mk_transition(protectedretired1, final1, Event::mk_free(true, ADR));
		mk_transition(protectedretired1, final1, Event::mk_free(false, ADR));
		mk_transition(entered1, init1, Event::mk_enter(f_protect1, true, OTHER));
		mk_transition(entered1, init1, Event::mk_enter(f_unprotect1, true, ADR));
		mk_transition(entered1, init1, Event::mk_enter(f_unprotect1, true, OTHER));
		mk_transition(exited1, init1, Event::mk_enter(f_protect1, true, OTHER));
		mk_transition(exited1, init1, Event::mk_enter(f_unprotect1, true, ADR));
		mk_transition(exited1, init1, Event::mk_enter(f_unprotect1, true, OTHER));
		mk_transition(protectedretired1, init1, Event::mk_enter(f_protect1, true, OTHER));
		mk_transition(protectedretired1, init1, Event::mk_enter(f_unprotect1, true, ADR));
		mk_transition(protectedretired1, init1, Event::mk_enter(f_unprotect1, true, OTHER));

		// done
		return std::make_unique<Observer>(std::move(states));
	}


	static std::unique_ptr<Observer> ebr_observer(const Program& prog) {
		// get functions to react on
		const auto& f_enterQ = find(prog, "enterQ");
		const auto& f_leaveQ = find(prog, "leaveQ");
		const auto& f_retire = find(prog, "retire");
		
		// state names
		std::string n_init        = "ebr:init";
		std::string n_leavingQ    = "ebr:leavingQ";
		std::string n_leftQ       = "ebr:leftQ";
		std::string n_leftretired = "ebr:retired";
		std::string n_final       = "ebr:freedprotected";
		std::string n_inQ         = "inv:inQ";
		std::string n_outQ        = "inv:outQ";
		std::string n_sink        = "inv:sink";

		// state vector
		std::vector<std::unique_ptr<State>> states;

		// ebr observer states
		states.push_back(mk_state_init(n_init));
		states.push_back(mk_state(n_leavingQ));
		states.push_back(mk_state(n_leftQ));
		states.push_back(mk_state(n_leftretired));
		states.push_back(mk_state_final(n_final));

		// invariant observer states
		states.push_back(mk_state_init(n_inQ));
		states.push_back(mk_state(n_outQ));
		states.push_back(mk_state_marked(n_sink));

		// get hold of the stats
		State& init        = *states[0];
		State& leavingQ    = *states[1];
		State& leftQ       = *states[2];
		State& leftretired = *states[3];
		State& final       = *states[4];
		State& inQ         = *states[5];
		State& outQ        = *states[6];
		State& sink        = *states[7];

		// shortcuts
		auto ADR = DataValue::DATA;
		auto OTHER = DataValue::OTHER;

		// ebr observer transitions
		mk_transition(init, leavingQ, Event::mk_enter(f_leaveQ, true, ADR));
		mk_transition(init, leavingQ, Event::mk_enter(f_leaveQ, true, OTHER));
		mk_transition(leavingQ, leftQ, Event::mk_exit(true));
		mk_transition(leftQ, leftretired, Event::mk_enter(f_retire, true, ADR));
		mk_transition(leftQ, leftretired, Event::mk_enter(f_retire, false, ADR));
		mk_transition(leftretired, final, Event::mk_free(true, ADR));
		mk_transition(leftretired, final, Event::mk_free(false, ADR));
		mk_transition(leavingQ, init, Event::mk_enter(f_enterQ, true, ADR));
		mk_transition(leavingQ, init, Event::mk_enter(f_enterQ, true, OTHER));
		mk_transition(leftQ, init, Event::mk_enter(f_enterQ, true, ADR));
		mk_transition(leftQ, init, Event::mk_enter(f_enterQ, true, OTHER));
		mk_transition(leftretired, init, Event::mk_enter(f_enterQ, true, ADR));
		mk_transition(leftretired, init, Event::mk_enter(f_enterQ, true, OTHER));

		// ebr observer transitions
		mk_transition(inQ, outQ, Event::mk_enter(f_leaveQ, true, ADR));
		mk_transition(inQ, outQ, Event::mk_enter(f_leaveQ, true, OTHER));
		mk_transition(outQ, inQ, Event::mk_enter(f_enterQ, true, ADR));
		mk_transition(outQ, inQ, Event::mk_enter(f_enterQ, true, OTHER));
		mk_transition(outQ, sink, Event::mk_enter(f_leaveQ, true, ADR));
		mk_transition(outQ, sink, Event::mk_enter(f_leaveQ, true, OTHER));

		// done
		return std::make_unique<Observer>(std::move(states));
	}

}