#pragma once

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "prog.hpp"


namespace tmr {

	enum class DataValue { DATA, OTHER };
	std::ostream& operator<<(std::ostream& os, const DataValue& val);

	/**
	 * ENTER events have: func, tid, dval
	 * EXIT events have :       tid
	 * FREE events have :            dval
	 */
	class Event {
		public:
			enum Type { ENTER, EXIT, FREE};
			const Type type;
			const Function* func;
			const bool from_offender;
			const DataValue dval;
			bool operator==(const Event& other) const;
			static Event mk_enter(const Function& func, bool from_offender, DataValue dval);
			static Event mk_exit(bool from_offender);
			static Event mk_free(DataValue dval);
			Event(Event& evt) = default;
			Event(const Event& evt) = default;
		private:
			Event(Type t, const Function* f, bool o, DataValue d) : type(t), func(f), from_offender(o), dval(d) {}
	};

	class State;

	class Transition {
		private:
			Event _trigger;
			const State& _next;
		public:
			Transition(Event trigger, const State& next) : _trigger(trigger), _next(next) {}
			const State& dst() const { return _next; }
			bool enabled(Event evt) const { return _trigger == evt; }
			bool same_trigger(const Transition& other) const { return _trigger == other._trigger; }
	};

	class State {
		private:
			std::string _name;
			bool _is_initial;
			bool _is_final;
			std::vector<std::unique_ptr<Transition>> _out;

		public:
			State(std::string name, bool is_initial, bool is_final) : _name(name), _is_initial(is_initial), _is_final(is_final) {}
			std::string name() const { return _name; }
			bool is_initial() const { return _is_initial; }
			bool is_final() const { return _is_final; }
			void add_transition(std::unique_ptr<Transition> transition);
			const State& next(Event evt) const;
	};

	class MultiState {
		private:
			std::vector<const State*> _states;

		public:
			MultiState();
			MultiState(std::vector<const State*> states);
			const std::vector<const State*>& states() const { return _states; }
			void print(std::ostream& os) const;
			bool is_final() const; // returns true if one state is final
			const State& find_final() const;
			MultiState next(Event evt) const;

			bool operator==(const MultiState& other) const;
			bool operator!=(const MultiState& other) const { return !(*this == other); }
			bool operator<(const MultiState& other) const;
	};

	static std::ostream& operator<<(std::ostream& os, const MultiState& state) {
		state.print(os);
		return os;
	}

	class Observer {
		private:
			std::vector<std::unique_ptr<State>> _states;
			MultiState _init;

		public:
			Observer(std::vector<std::unique_ptr<State>> states);
			const MultiState& initial_state() const { return _init; }
	};

}
