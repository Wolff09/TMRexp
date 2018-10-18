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
			const bool thread;
			const DataValue dval;
			bool operator==(const Event& other) const;
			static Event mk_enter(const Function& func, bool thread, DataValue dval);
			static Event mk_exit(bool thread);
			static Event mk_free(bool thread, DataValue dval);
			Event(Event& evt) = default;
			Event(const Event& evt) = default;
		private:
			Event(Type t, const Function* f, bool b, DataValue d) : type(t), func(f), thread(b), dval(d) {}
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
			bool _is_marked;
			bool _is_colored;
			std::size_t _color;
			std::vector<std::unique_ptr<Transition>> _out;

		public:
			State(std::string name, bool is_initial, bool is_final)
			  : _name(name), _is_initial(is_initial), _is_final(is_final), _is_marked(false), _is_colored(false), _color(0) {}
			State(std::string name, bool is_initial, bool is_final, bool is_marked)
			  : _name(name), _is_initial(is_initial), _is_final(is_final), _is_marked(is_marked), _is_colored(false), _color(0) {}
			State(std::string name, bool is_initial, bool is_final, bool is_marked, std::size_t color) 
			  : _name(name), _is_initial(is_initial), _is_final(is_final), _is_marked(is_marked), _is_colored(true), _color(color) {}
			std::string name() const { return _name; }
			bool is_initial() const { return _is_initial; }
			bool is_final() const { return _is_final; }
			bool is_marked() const { return _is_marked; } // used to mark states which resemble a usage invariant violation
			bool is_colored() const { return _is_colored; }
			bool color() const { assert(is_colored()); return _color; } // used to prune false-positive interference; states with same color cannot occur in victim and interferer
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
			bool is_marked() const; // returns true if one state is marked
			bool is_colored() const; // returns true if one state is colored
			bool colors_intersect(const MultiState& other) const;
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
