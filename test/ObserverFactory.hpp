#pragma once

#include <memory>
#include <assert.h>
#include "prog.hpp"
#include "observer.hpp"
#include "config.hpp"

namespace tmr {

	static std::unique_ptr<State> mk_state(std::string name, bool is_initial, bool is_final, bool is_special=false, bool is_marked=false) {
		return std::make_unique<State>(name, is_initial, is_final, is_special, is_marked);
	}

	static void add_trans(State& src, const State& dst, const Function& evt, OValue ov = OValue::Empty()) {
		Guard g({ ov });
		src.add_transition(std::make_unique<Transition>(evt, g, dst));
	}

	static void add_trans(State& src, const State& dst, const Function& evt, std::size_t ovid) {
		Guard g({ Equality(src.observer().mk_var(ovid)) });
		src.add_transition(std::make_unique<Transition>(evt, g, dst));
	}

	static std::unique_ptr<Observer> stack_observer(const Function& in, const Function& out, const Function& free) {
		// special state names
		std::string n_sink = "sink";
		std::string n_make = "output was never input";
		std::string n_dupl = "duplicate output";
		std::string n_loss = "value loss";
		std::string n_lifo = "no lifo";

		// make states
		std::vector<std::unique_ptr<State>> states;
		states.push_back(mk_state(n_sink, false, false));

		// ObsMake
		states.push_back(mk_state("u0", true, false));
		states.push_back(mk_state(n_make, false, true));

		// ObsDupl
		states.push_back(mk_state("u1", true, false));
		states.push_back(mk_state("u2", false, false));
		states.push_back(mk_state("u3", false, false));
		states.push_back(mk_state(n_dupl, false, true));

		// ObsLoss
		states.push_back(mk_state("u4", true, false));
		states.push_back(mk_state("u5", false, false));
		states.push_back(mk_state(n_loss, false, true));

		// ObsLifo
		states.push_back(mk_state("u6", true, false));
		states.push_back(mk_state("u7", false, false));
		states.push_back(mk_state("u8", false, false));
		states.push_back(mk_state(n_lifo, false, true));


		// get hold of the stats
		const State& sink       = *states[0];  // n_sink
		      State& u0     	= *states[1];  // u0
		const State& f_make 	= *states[2];  // n_make
		      State& u1     	= *states[3];  // u1
		      State& u2     	= *states[4];  // u2
		      State& u3     	= *states[5];  // u3
		const State& f_dupl 	= *states[6];  // n_dupl
		      State& u4     	= *states[7];  // u4
		      State& u5     	= *states[8];  // u5
		const State& f_loss 	= *states[9];  // n_loss
		      State& u6     	= *states[10]; // u6
		      State& u7     	= *states[11]; // u7
		      State& u8     	= *states[12]; // u8
		const State& f_lifo 	= *states[13]; // n_lifo


		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 2));


		// add transitions

		// ObsMake a = 0 [u*]
		add_trans(u0, f_make, out, 0);
		add_trans(u0, sink, in, 0);

		// ObsDupl a = 0 [u*]
		add_trans(u1, u2, in, 0);
		add_trans(u1, sink, out, 0);
		add_trans(u2, u3, out, 0);
		add_trans(u2, sink, in, 0);
		add_trans(u3, f_dupl, out, 0);
		add_trans(u3, sink, in, 0);

		// ObsLoss a = 0 [u*]
		add_trans(u4, u5, in, 0);
		add_trans(u4, sink, out, 0);
		add_trans(u5, f_loss, out);
		add_trans(u5, sink, out, 0);

		// ObsLifo a = 0, b = 1
		add_trans(u6, u7, in, 0);
		add_trans(u6, sink, in, 1);
		add_trans(u6, sink, out, 0);
		add_trans(u6, sink, out, 1);
		add_trans(u7, u8, in, 1);
		add_trans(u7, sink, in, 0);
		add_trans(u7, sink, out, 0);
		add_trans(u7, sink, out, 1);
		add_trans(u8, f_lifo, out, 0);
		add_trans(u8, sink, out, 1);
		add_trans(u8, sink, in, 0);
		add_trans(u8, sink, in, 1);


		// make observer
		return result;
	}

	static std::unique_ptr<Observer> queue_observer(const Function& in, const Function& out, const Function& free) {
		// special state names
		std::string n_sink = "sink";
		std::string n_make = "output was never input";
		std::string n_dupl = "duplicate output";
		std::string n_loss = "value loss";
		std::string n_fifo = "no fifo";

		// make states
		std::vector<std::unique_ptr<State>> states;
		states.push_back(mk_state(n_sink, false, false));

		// ObsMake
		states.push_back(mk_state("u0", true, false));
		states.push_back(mk_state(n_make, false, true));

		// ObsDupl
		states.push_back(mk_state("u1", true, false));
		states.push_back(mk_state("u2", false, false));
		states.push_back(mk_state("u3", false, false));
		states.push_back(mk_state(n_dupl, false, true));

		// ObsLoss
		states.push_back(mk_state("u4", true, false));
		states.push_back(mk_state("u5", false, false));
		states.push_back(mk_state(n_loss, false, true));

		// ObsLifo
		states.push_back(mk_state("u6", true, false));
		states.push_back(mk_state("u7", false, false));
		states.push_back(mk_state("u8", false, false));
		states.push_back(mk_state(n_fifo, false, true));


		// get hold of the stats
		const State& sink       = *states[0];  // n_sink
		      State& u0     	= *states[1];  // u0
		const State& f_make 	= *states[2];  // n_make
		      State& u1     	= *states[3];  // u1
		      State& u2     	= *states[4];  // u2
		      State& u3     	= *states[5];  // u3
		const State& f_dupl 	= *states[6];  // n_dupl
		      State& u4     	= *states[7];  // u4
		      State& u5     	= *states[8];  // u5
		const State& f_loss 	= *states[9];  // n_loss
		      State& u6     	= *states[10]; // u6
		      State& u7     	= *states[11]; // u7
		      State& u8     	= *states[12]; // u8
		const State& f_fifo 	= *states[13]; // n_fifo


		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 2));


		// add transitions

		// ObsMake a = 0 [u*]
		add_trans(u0, f_make, out, 0);
		add_trans(u0, sink, in, 0);

		// ObsDupl a = 0 [u*]
		add_trans(u1, u2, in, 0);
		add_trans(u1, sink, out, 0);
		add_trans(u2, u3, out, 0);
		add_trans(u2, sink, in, 0);
		add_trans(u3, f_dupl, out, 0);
		add_trans(u3, sink, in, 0);

		// ObsLoss a = 0 [u*]
		add_trans(u4, u5, in, 0);
		add_trans(u4, sink, out, 0);
		add_trans(u5, f_loss, out);
		add_trans(u5, sink, out, 0);

		// ObsFifo a = 0, b = 1
		add_trans(u6, u7, in, 0);
		add_trans(u6, sink, in, 1);
		add_trans(u6, sink, out, 0);
		add_trans(u6, sink, out, 1);
		add_trans(u7, u8, in, 1);
		add_trans(u7, sink, in, 0);
		add_trans(u7, sink, out, 0);
		add_trans(u7, sink, out, 1);
		add_trans(u8, f_fifo, out, 1);
		add_trans(u8, sink, out, 0);
		add_trans(u8, sink, in, 0);
		add_trans(u8, sink, in, 1);


		// make observer
		return result;
	}

	static std::unique_ptr<Observer> smr_observer(const Function& guard, const Function& unguard, const Function& retire, const Function& free) {
		std::vector<std::unique_ptr<State>> states;
		
		states.push_back(mk_state("s0", true, false));
		states.push_back(mk_state("g", false, false));
		states.push_back(mk_state("gr", false, false, true));
		states.push_back(mk_state("r", false, false, true));
		states.push_back(mk_state("rg", false, false, true));
		states.push_back(mk_state("f", false, true));
		#if MERGE_VALID_PTR
			states.push_back(mk_state("d", false, false, false, true));
			states.push_back(mk_state("dg", false, false, false, true));
		#endif

		State& s0 = *states[0];
		State& sG = *states[1];
		State& sGR = *states[2];
		State& sR = *states[3];
		State& sRG = *states[4];
		State& sF = *states[5];
		#if MERGE_VALID_PTR
			State& sD = *states[6];
			State& sDG = *states[7];
		#endif

		add_trans(s0, sG, guard, OValue::Anonymous());
		add_trans(sG, sGR, retire, OValue::Anonymous());
		add_trans(sG, sF, free, OValue::Anonymous());
		add_trans(sGR, sF, free, OValue::Anonymous());
		add_trans(sG, s0, unguard, OValue::Anonymous());
		add_trans(sGR, s0, unguard, OValue::Anonymous());

		add_trans(s0, sF, free, OValue::Anonymous());
		add_trans(s0, sR, retire, OValue::Anonymous());
		add_trans(sR, sRG, guard, OValue::Anonymous());
		add_trans(sRG, sR, unguard, OValue::Anonymous());

		#if MERGE_VALID_PTR
			add_trans(sR, sD, free, OValue::Anonymous());
			add_trans(sD, sR, retire, OValue::Anonymous());
			add_trans(sD, sDG, guard, OValue::Anonymous());
			add_trans(sD, sF, free, OValue::Anonymous());
			add_trans(sRG, sDG, free, OValue::Anonymous());
			add_trans(sDG, sGR, retire, OValue::Anonymous());
			add_trans(sDG, sD, unguard, OValue::Anonymous());
			add_trans(sDG, sF, free, OValue::Anonymous());
		#else
			add_trans(sR, s0, free, OValue::Anonymous());
			add_trans(sRG, sG, free, OValue::Anonymous());
		#endif

		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 0));
		return result;
	}

	static std::unique_ptr<Observer> ebr_observer(const Function& enterQ, const Function& leaveQ, const Function& retire, const Function& free) {
		std::vector<std::unique_ptr<State>> states;
		
		states.push_back(mk_state("q1", true, false));
		states.push_back(mk_state("q2", false, false, true));
		states.push_back(mk_state("n1", false, false));
		states.push_back(mk_state("n2", false, false, true));
		states.push_back(mk_state("t", false, false, true));
		states.push_back(mk_state("f", false, true));

		State& q1 = *states[0];
		State& q2 = *states[1];
		State& n1 = *states[2];
		State& n2 = *states[3];
		State& st = *states[4];
		State& sf = *states[5];

		add_trans(q1, q2, retire, OValue::Anonymous());
		add_trans(q1, sf, free, OValue::Anonymous());
		add_trans(q1, n1, enterQ, OValue::Anonymous());
		
		add_trans(q2, sf, free, OValue::Anonymous());
		add_trans(q2, n2, enterQ, OValue::Anonymous());

		add_trans(n1, q1, leaveQ, OValue::Anonymous());
		add_trans(n1, n2, retire, OValue::Anonymous());
		add_trans(n1, sf, free, OValue::Anonymous());

		add_trans(n2, n1, free, OValue::Anonymous());
		add_trans(n2, st, leaveQ, OValue::Anonymous());

		add_trans(st, q1, free, OValue::Anonymous());
		add_trans(st, n2, enterQ, OValue::Anonymous());

		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 0));
		return result;
	}

	static std::unique_ptr<Observer> no_reclamation_observer(const Function& free) {
		std::vector<std::unique_ptr<State>> states;
		
		states.push_back(mk_state("q1", true, false));
		states.push_back(mk_state("f", false, true));

		State& s0 = *states[0];
		State& sf = *states[1];

		add_trans(s0, sf, free, OValue::Anonymous());

		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 0));
		return result;
	}

	static std::unique_ptr<Observer> all_reclamation_observer() {
		std::vector<std::unique_ptr<State>> states;
		
		states.push_back(mk_state("q1", true, false));

		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 0));
		return result;
	}

}