#pragma once

#include <memory>
#include <assert.h>
#include "prog.hpp"
#include "observer.hpp"

namespace tmr {

	static std::unique_ptr<State> mk_state(std::string name, bool is_initial, bool is_final) {
		return std::make_unique<State>(name, is_initial, is_final);
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
		std::string n_dfree = "double free";

		// make states
		std::vector<std::unique_ptr<State>> states;
		states.push_back(mk_state(n_sink, false, false));

		// ObsMake
		states.push_back(mk_state("u0", true, false));
		states.push_back(mk_state("v0", true, false));
		states.push_back(mk_state(n_make, false, true));

		// ObsDupl
		states.push_back(mk_state("u1", true, false));
		states.push_back(mk_state("v1", true, false));
		states.push_back(mk_state("u2", false, false));
		states.push_back(mk_state("v2", false, false));
		states.push_back(mk_state("u3", false, false));
		states.push_back(mk_state("v3", false, false));
		states.push_back(mk_state(n_dupl, false, true));

		// ObsLoss
		states.push_back(mk_state("u4", true, false));
		states.push_back(mk_state("v4", true, false));
		states.push_back(mk_state("u5", false, false));
		states.push_back(mk_state("v5", false, false));
		states.push_back(mk_state(n_loss, false, true));

		// ObsLifo
		states.push_back(mk_state("u6", true, false));
		states.push_back(mk_state("v6", true, false));
		states.push_back(mk_state("u7", false, false));
		states.push_back(mk_state("v7", false, false));
		states.push_back(mk_state("u8", false, false));
		states.push_back(mk_state("v8", false, false));
		states.push_back(mk_state(n_lifo, false, true));

		// ObsFree
		states.push_back(mk_state("u9", true, false));
		states.push_back(mk_state("v9", true, false));
		states.push_back(mk_state("u10", false, false));
		states.push_back(mk_state("v10", false, false));
		states.push_back(mk_state(n_dfree, false, true));


		// get hold of the stats
		const State& sink       = *states[0];  assert(sink.name()   == n_sink);
		      State& u0     	= *states[1]; assert(u0.name()     == "u0");
		      State& v0     	= *states[2]; assert(v0.name()     == "v0");
		const State& f_make 	= *states[3]; assert(f_make.name() == n_make);
		      State& u1     	= *states[4]; assert(u1.name()     == "u1");
		      State& v1     	= *states[5]; assert(v1.name()     == "v1");
		      State& u2     	= *states[6]; assert(u2.name()     == "u2");
		      State& v2     	= *states[7]; assert(v2.name()     == "v2");
		      State& u3     	= *states[8]; assert(u3.name()     == "u3");
		      State& v3     	= *states[9]; assert(v3.name()     == "v3");
		const State& f_dupl 	= *states[10]; assert(f_dupl.name() == n_dupl);
		      State& u4     	= *states[11]; assert(u4.name()     == "u4");
		      State& v4     	= *states[12]; assert(v4.name()     == "v4");
		      State& u5     	= *states[13]; assert(u5.name()     == "u5");
		      State& v5     	= *states[14]; assert(v5.name()     == "v5");
		const State& f_loss 	= *states[15]; assert(f_loss.name() == n_loss);
		      State& u6     	= *states[16]; assert(u6.name()     == "u6");
		      State& v6     	= *states[17]; assert(v6.name()     == "v6");
		      State& u7     	= *states[18]; assert(u7.name()     == "u7");
		      State& v7     	= *states[19]; assert(v7.name()     == "v7");
		      State& u8     	= *states[20]; assert(u8.name()     == "u8");
		      State& v8     	= *states[21]; assert(v8.name()     == "v8");
		const State& f_lifo 	= *states[22]; assert(f_lifo.name() == n_lifo);
		      State& u9     	= *states[23]; assert(u9.name()     == "u9");
		      State& v9     	= *states[24]; assert(v9.name()     == "v9");
		      State& u10    	= *states[25]; assert(u10.name()     == "u10");
		      State& v10    	= *states[26]; assert(v10.name()     == "v10");
		const State& f_dfree	= *states[27]; assert(f_dfree.name() == n_dfree);


		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 2));


		// add transitions

		// ObsMake a = 0 [u*]
		add_trans(u0, f_make, out, 0);
		add_trans(u0, sink, in, 0);

		// ObsMake a = 1 [v*]
		add_trans(v0, f_make, out, 1);
		add_trans(v0, sink, in, 1);


		// ObsDupl a = 0 [u*]
		add_trans(u1, u2, in, 0);
		add_trans(u1, sink, out, 0);
		add_trans(u2, u3, out, 0);
		add_trans(u2, sink, in, 0);
		add_trans(u3, f_dupl, out, 0);
		add_trans(u3, sink, in, 0);

		// ObsDupl a = 1 [v*]
		add_trans(v1, v2, in, 1);
		add_trans(v1, sink, out, 1);
		add_trans(v2, v3, out, 1);
		add_trans(v2, sink, in, 1);
		add_trans(v3, f_dupl, out, 1);
		add_trans(v3, sink, in, 1);


		// ObsLoss a = 0 [u*]
		add_trans(u4, u5, in, 0);
		add_trans(u4, sink, out, 0);
		add_trans(u5, f_loss, out);
		add_trans(u5, sink, out, 0);

		// ObsLoss a = 1 [v*]
		add_trans(v4, v5, in, 1);
		add_trans(v4, sink, out, 1);
		add_trans(v5, f_loss, out);
		add_trans(v5, sink, out, 1);


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

		// ObsLifo a = 1, b = 0
		add_trans(v6, v7, in, 1);
		add_trans(v6, sink, in, 0);
		add_trans(v6, sink, out, 1);
		add_trans(v6, sink, out, 0);
		add_trans(v7, v8, in, 0);
		add_trans(v7, sink, in, 1);
		add_trans(v7, sink, out, 1);
		add_trans(v7, sink, out, 0);
		add_trans(v8, f_lifo, out, 1);
		add_trans(v8, sink, out, 0);
		add_trans(v8, sink, in, 1);
		add_trans(v8, sink, in, 0);

		// ObsFree a = 0, b = 1
		add_trans(u9, u10, free, 0);
		add_trans(u10, f_dfree, free, 0);
		
		// ObsFree a = 1, b = 0
		add_trans(v9, v10, free, 1);
		add_trans(v10, f_dfree, free, 1);


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
		std::string n_dfree = "double free";

		// make states
		std::vector<std::unique_ptr<State>> states;
		states.push_back(mk_state(n_sink, false, false));

		// ObsMake
		states.push_back(mk_state("u0", true, false));
		states.push_back(mk_state("v0", true, false));
		states.push_back(mk_state(n_make, false, true));

		// ObsDupl
		states.push_back(mk_state("u1", true, false));
		states.push_back(mk_state("v1", true, false));
		states.push_back(mk_state("u2", false, false));
		states.push_back(mk_state("v2", false, false));
		states.push_back(mk_state("u3", false, false));
		states.push_back(mk_state("v3", false, false));
		states.push_back(mk_state(n_dupl, false, true));

		// ObsLoss
		states.push_back(mk_state("u4", true, false));
		states.push_back(mk_state("v4", true, false));
		states.push_back(mk_state("u5", false, false));
		states.push_back(mk_state("v5", false, false));
		states.push_back(mk_state(n_loss, false, true));

		// ObsLifo
		states.push_back(mk_state("u6", true, false));
		states.push_back(mk_state("v6", true, false));
		states.push_back(mk_state("u7", false, false));
		states.push_back(mk_state("v7", false, false));
		states.push_back(mk_state("u8", false, false));
		states.push_back(mk_state("v8", false, false));
		states.push_back(mk_state(n_fifo, false, true));

		// ObsFree // TODO: remove?
		states.push_back(mk_state("u9", true, false));
		states.push_back(mk_state("v9", true, false));
		states.push_back(mk_state("u10", false, false));
		states.push_back(mk_state("v10", false, false));
		states.push_back(mk_state(n_dfree, false, true));


		// get hold of the stats
		const State& sink       = *states[0];  assert(sink.name()   == n_sink);
		      State& u0     	= *states[1]; assert(u0.name()     == "u0");
		      State& v0     	= *states[2]; assert(v0.name()     == "v0");
		const State& f_make 	= *states[3]; assert(f_make.name() == n_make);
		      State& u1     	= *states[4]; assert(u1.name()     == "u1");
		      State& v1     	= *states[5]; assert(v1.name()     == "v1");
		      State& u2     	= *states[6]; assert(u2.name()     == "u2");
		      State& v2     	= *states[7]; assert(v2.name()     == "v2");
		      State& u3     	= *states[8]; assert(u3.name()     == "u3");
		      State& v3     	= *states[9]; assert(v3.name()     == "v3");
		const State& f_dupl 	= *states[10]; assert(f_dupl.name() == n_dupl);
		      State& u4     	= *states[11]; assert(u4.name()     == "u4");
		      State& v4     	= *states[12]; assert(v4.name()     == "v4");
		      State& u5     	= *states[13]; assert(u5.name()     == "u5");
		      State& v5     	= *states[14]; assert(v5.name()     == "v5");
		const State& f_loss 	= *states[15]; assert(f_loss.name() == n_loss);
		      State& u6     	= *states[16]; assert(u6.name()     == "u6");
		      State& v6     	= *states[17]; assert(v6.name()     == "v6");
		      State& u7     	= *states[18]; assert(u7.name()     == "u7");
		      State& v7     	= *states[19]; assert(v7.name()     == "v7");
		      State& u8     	= *states[20]; assert(u8.name()     == "u8");
		      State& v8     	= *states[21]; assert(v8.name()     == "v8");
		const State& f_fifo 	= *states[22]; assert(f_fifo.name() == n_fifo);
		      State& u9     	= *states[23]; assert(u9.name()     == "u9");
		      State& v9     	= *states[24]; assert(v9.name()     == "v9");
		      State& u10    	= *states[25]; assert(u10.name()     == "u10");
		      State& v10    	= *states[26]; assert(v10.name()     == "v10");
		const State& f_dfree	= *states[27]; assert(f_dfree.name() == n_dfree);


		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 2));


		// add transitions

		// ObsMake a = 0 [u*]
		add_trans(u0, f_make, out, 0);
		add_trans(u0, sink, in, 0);

		// ObsMake a = 1 [v*]
		add_trans(v0, f_make, out, 1);
		add_trans(v0, sink, in, 1);


		// ObsDupl a = 0 [u*]
		add_trans(u1, u2, in, 0);
		add_trans(u1, sink, out, 0);
		add_trans(u2, u3, out, 0);
		add_trans(u2, sink, in, 0);
		add_trans(u3, f_dupl, out, 0);
		add_trans(u3, sink, in, 0);

		// ObsDupl a = 1 [v*]
		add_trans(v1, v2, in, 1);
		add_trans(v1, sink, out, 1);
		add_trans(v2, v3, out, 1);
		add_trans(v2, sink, in, 1);
		add_trans(v3, f_dupl, out, 1);
		add_trans(v3, sink, in, 1);


		// ObsLoss a = 0 [u*]
		add_trans(u4, u5, in, 0);
		add_trans(u4, sink, out, 0);
		add_trans(u5, f_loss, out);
		add_trans(u5, sink, out, 0);

		// ObsLoss a = 1 [v*]
		add_trans(v4, v5, in, 1);
		add_trans(v4, sink, out, 1);
		add_trans(v5, f_loss, out);
		add_trans(v5, sink, out, 1);


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

		// ObsFifo a = 1, b = 0
		add_trans(v6, v7, in, 1);
		add_trans(v6, sink, in, 0);
		add_trans(v6, sink, out, 1);
		add_trans(v6, sink, out, 0);
		add_trans(v7, v8, in, 0);
		add_trans(v7, sink, in, 1);
		add_trans(v7, sink, out, 1);
		add_trans(v7, sink, out, 0);
		add_trans(v8, f_fifo, out, 0);
		add_trans(v8, sink, out, 1);
		add_trans(v8, sink, in, 1);
		add_trans(v8, sink, in, 0);

		// ObsFree a = 0, b = 1
		add_trans(u9, u10, free, 0);
		add_trans(u10, f_dfree, free, 0);
		
		// ObsFree a = 1, b = 0
		add_trans(v9, v10, free, 1);
		add_trans(v10, f_dfree, free, 1);


		// make observer
		return result;
	}

	static std::unique_ptr<Observer> smr_observer(const Function& guard, const Function& unguard, const Function& retire, const Function& free) {
		std::vector<std::unique_ptr<State>> states;
		
		states.push_back(mk_state("s0", true, false));
		states.push_back(mk_state("s1-g", false, false));
		states.push_back(mk_state("s2-gr", false, false));
		states.push_back(mk_state("s3-f", false, true));

		State& s0 = *states[0];
		State& s1 = *states[1];
		State& s2 = *states[2];
		State& s3 = *states[3];

		add_trans(s0, s1, guard, OValue::Anonymous());
		add_trans(s1, s2, retire, OValue::Anonymous());
		add_trans(s2, s3, free, OValue::Anonymous());
		add_trans(s1, s0, unguard, OValue::Anonymous());
		add_trans(s2, s0, unguard, OValue::Anonymous());

		auto result = std::unique_ptr<Observer>(new Observer(std::move(states), 0));
		return result;
	}

}