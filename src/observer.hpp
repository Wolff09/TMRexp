#pragma once

#include <map>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include "prog.hpp"


namespace tmr {

	class OValue {
		public:
			enum Type { ANONYMOUS, EMPTY, OBSERVABLE, DUMMY };
			Type type() const { return _type; }
			std::size_t id() const { assert(_type == OBSERVABLE); return _id; }
			bool operator==(const OValue& other) const;
			bool operator<(const OValue& other) const;
			static inline OValue Empty() { return OValue(EMPTY, 0); }
			static inline OValue Anonymous() { return OValue(ANONYMOUS, 0); }
			OValue() : _type(DUMMY), _id(0) {}
			void print(std::ostream& os) const;
		private:
			OValue(Type type, std::size_t id) : _type(type), _id(id) {}
			static inline OValue Observable(std::size_t id) { return OValue(OBSERVABLE, id); }
			Type _type;
			std::size_t _id;

		friend class Observer;
	};

	std::ostream& operator<<(std::ostream& os, const OValue& ov);


	/**
	 * @brief Class representing an equality or inequality.
	 * @details The left-hand side of the equality is a variable the value of
	 *          which is provided to the ``eval`` member and not to the constructor.
	 */
	struct Equality {
		const OValue rhs;
		const bool negated;
		Equality(OValue rhs);
		Equality(OValue rhs, bool negated);
		bool eval(OValue lhs) const;
	};


	struct Guard {
		std::vector<Equality> eqs;
		Guard(std::vector<Equality> equalities);
		bool eval(OValue oval) const;
	};


	class State;

	struct Transition {
		const Function& evt;
		const Guard guard;
		const State& next;
		Transition(const Function& fun, const Guard guard, const State& next);
		bool enabled(const Function& event, OValue oval) const;
	};


	class Observer;

	class State {
		private:
			const Observer* _obs;
			std::string _name;
			bool _is_initial;
			bool _is_final;
			bool _is_special = false;
			std::vector<std::unique_ptr<Transition>> _out;
			std::size_t _id;

		public:
			State(std::string name, bool is_initial, bool is_final);
			State(std::string name, bool is_initial, bool is_final, bool is_special);
			std::string name() const { return _name; }
			std::size_t id() const { return _id; }
			bool is_initial() const { return _is_initial; }
			bool is_final() const { return _is_final; }
			bool is_special() const { return _is_special; }
			const Observer& observer() const { assert(_obs != NULL); return *_obs; }
			void add_transition(std::unique_ptr<Transition> trans) { _out.push_back(std::move(trans)); }
			/**
			 * @brief Computes the successor/post state for a given linearisation point under the specified
			 *        data values. Returns ``*this`` state if no successor/post state was found.
			 * @details One has to specify the data values for the formal parameter and the return value.
			 *          If the event function from the linearisation point has no formal parameter, the given
			 *          value is ignored, i.e. an arbitrary value can be passed (you may prefer an empty ``Maybe()``).
			 *          The analogous is true for an absent return value.
			 */
			const State& next(const Function& evt_name, OValue evt_val) const;


		friend class Observer;
	};


	class MultiState {
		private:
			std::vector<const State*> _states;

		public:
			MultiState();
			MultiState(std::vector<const State*> states);
			const std::vector<const State*>& states() const { return _states; }
			void print(std::ostream& os) const;
			/**
			 * @brief Computes the successor/post state for given linearisation point under the specified data
			 *        values for all ``State``s represented by this ``MultiState``.
			 * @see State
			 */
			MultiState next(const Function& evt_name, OValue evt_val) const;
			/**
			 * @brief Checks whether or not one of the ``State``s represented by this ``MultiState`` is final.
			 */
			bool is_final() const;
			/**
			 * @brief If this ``MultiState`` is final, gives some final ``State`` which is represent by this ``MultiState``.
			 */
			const State& find_final() const;
			const Observer& observer() const { assert(_states.size() > 0); return _states[0]->observer(); }

			bool operator==(const MultiState& other) const;
			bool operator<(const MultiState& other) const;
	};

	static std::ostream& operator<<(std::ostream& os, const MultiState& state) {
		state.print(os);
		return os;
	}


	class Observer {
		private:
			std::vector<std::unique_ptr<State>> _states;
			std::size_t _numVars;
			MultiState _init;

			bool has_final() const;
			bool has_initial() const;

		public:
			Observer(std::vector<std::unique_ptr<State>> states, std::size_t numVars);
			std::size_t numVars() const { return _numVars; }
			OValue mk_var(std::size_t index) const { assert(index < _numVars); return OValue::Observable(index); }
			const MultiState& initial_state() const { return _init; }
	};

}
