#include "observer.hpp"

#include <stdexcept>
#include <algorithm>

using namespace tmr;


/************************ VALUES ************************/

bool OValue::operator==(const OValue& other) const {
	return _type == other._type && _id == other._id;
}

bool OValue::operator<(const OValue& other) const {
	if (_type < other._type) return true;
	if (_type > other._type) return false;
	assert(_type == other._type);
	return _type == OBSERVABLE && _id < other._id;
}

void OValue::print(std::ostream& os) const {
	os << "<";
	switch (_type) {
		case EMPTY: os << "empty"; break;
		case ANONYMOUS: os << "?"; break;
		case OBSERVABLE: os << _id; break;
		case DUMMY: os << "_"; break;
	}
	os << ">";
}

std::ostream& tmr::operator<<(std::ostream& os, const OValue& ov) {
	ov.print(os);
	return os;
}


/************************ EQUALITY  ************************/

Equality::Equality(OValue rhs, bool negated) : rhs(rhs), negated(negated) {
	assert(rhs.type() == OValue::EMPTY || rhs.type() == OValue::OBSERVABLE);
}

Equality::Equality(OValue rhs) : Equality(rhs, false) {}

bool Equality::eval(OValue lhs) const {
	return (lhs == rhs) ^ negated;
}


/************************ GUARD ************************/

Guard::Guard(std::vector<Equality> equalities) : eqs(equalities) {}

bool Guard::eval(OValue oval) const {
	for (const auto& e : eqs)
		if (!e.eval(oval))
			return false;
	return true;
}


/************************ TRANSITION ************************/

Transition::Transition(const Function& fun, const Guard guard, const State& next) : evt(fun), guard(guard), next(next) {}

bool Transition::enabled(const Function& event, OValue oval) const {
	return &evt == &event && guard.eval(oval);
}


/************************ STATE ************************/

State::State(std::string name, bool is_initial, bool is_final) : _name(name), _is_initial(is_initial), _is_final(is_final) {}

State::State(std::string name, bool is_initial, bool is_final, bool is_special) : _name(name), _is_initial(is_initial), _is_final(is_final), _is_special(is_special) {}

State::State(std::string name, bool is_initial, bool is_final, bool is_special, bool is_marked) : _name(name), _is_initial(is_initial), _is_final(is_final), _is_special(is_special), _is_marked(is_marked) {}

const State& State::next(const Function& evt_name, OValue evt_val) const {
	// search for an enabled transition (assuming there is at most one)
	for (const auto& t : _out)
		if (t->enabled(evt_name, evt_val))
			return t->next;

	// if no enabled transition was found we stay at the current state
	return *this;
}


/************************ MULTISET ************************/

MultiState::MultiState() {}

MultiState::MultiState(std::vector<const State*> states) : _states(states) {}

MultiState MultiState::next(const Function& evt_name, OValue evt_val) const {
	MultiState result;
	result._states.reserve(_states.size());
	for (const State* s : _states) {
		assert(s != NULL);
		result._states.push_back(&s->next(evt_name, evt_val));
	}
	return result;
}

bool MultiState::is_final() const {
	for (const State* s : _states)
		if (s->is_final())
			return true;
	return false;
}

const State& MultiState::find_final() const {
	assert(is_final());
	for (const State* s : _states)
		if (s->is_final())
			return *s;

	assert(false);
	throw std::logic_error("Malicious call to MultiState::find_final()");
}

bool MultiState::operator==(const MultiState& other) const {
	for (std::size_t i = 0; i < _states.size(); i++)
		if (_states[i] != other._states[i])
			return false;
	return true;
}

bool MultiState::operator<(const MultiState& other) const {
	for (std::size_t i = 0; i < _states.size(); i++)
		if (_states[i] < other._states[i])
			return true;
		else if (_states[i] > other._states[i])
			return false;
	return false;
}

void MultiState::print(std::ostream& os) const {
	os << "{ ";
	if (_states.size() > 0) {
		os << _states[0]->name();
		for (std::size_t i = 1; i < _states.size(); i++)
			os << ", " << _states[i]->name();
	}
	os << " }";
}

/************************ OBSERVER ************************/

bool Observer::has_final() const {
	for (const auto& s : _states)
		if (s->is_final()) return true;
	return false;
}

bool Observer::has_initial() const {
	for (const auto& s : _states)
		if (s->is_initial()) return true;
	return false;
}

Observer::Observer(std::vector<std::unique_ptr<State>> states, std::size_t numVars) : _states(std::move(states)), _numVars(numVars) {
	// tell them who's their daddy
	for (const auto& s : _states)
		s->_obs = this;

	// check for initial/final states
	assert(std::count_if(_states.begin(), _states.end(), [](const std::unique_ptr<State>& s){ return s->is_initial(); }) > 0);
	assert(std::count_if(_states.begin(), _states.end(), [](const std::unique_ptr<State>& s){ return s->is_final(); }) > 0);

	// make initial state
	std::vector<const State*> init;
	for (const auto& s : _states)
		if (s->is_initial())
			init.push_back(s.get());
	_init = MultiState(std::move(init));


	for (std::size_t i = 0; i < _states.size(); i++)
		_states.at(i)->_id = i;
}

