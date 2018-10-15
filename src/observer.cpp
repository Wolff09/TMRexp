#include "observer.hpp"

#include <stdexcept>
#include <algorithm>

using namespace tmr;


/************************ VALUES ************************/

std::ostream& tmr::operator<<(std::ostream& os, const DataValue& val) {
	os << "<";
	switch (val) {
		case DataValue::DATA: os << "D"; break;
		case DataValue::OTHER: os << "?"; break;
	}
	os << ">";
	return os;
}


/************************ VALUES ************************/

bool Event::operator==(const Event& other) const {
	return type == other.type && func == other.func && tid == other.tid && dval == other.dval;
}

Event Event::mk_enter(const Function& func, unsigned int tid, DataValue dval) {
	return Event(ENTER, &func, tid, dval);
}

Event Event::mk_exit(unsigned int tid) {
	return Event(EXIT, nullptr, tid, DataValue::DATA);
}

Event Event::mk_free(DataValue dval) {
	return Event(FREE, nullptr, 0, dval);
}


/************************ STATE ************************/

void State::add_transition(std::unique_ptr<Transition> transition) {
	 // TODO: ensure determinism { 
	for (const auto& trans : _out) {
		if (transition->same_trigger(*trans)) {
			throw std::logic_error("Non-deterministic observers are not supported");
		}
	}
	_out.push_back(std::move(transition));
}

const State& State::next(Event evt) const {
	for (const auto& trans : _out) {
		if (trans->enabled(evt)) {
			return trans->dst();
		}
	}

	// if no enabled transition was found we stay at the current state
	return *this;
}


/************************ MULTISET ************************/

MultiState::MultiState() {}

MultiState::MultiState(std::vector<const State*> states) : _states(states) {}

MultiState MultiState::next(Event evt) const {
	MultiState result;
	result._states.reserve(_states.size());
	for (const State* s : _states) {
		assert(s != NULL);
		result._states.push_back(&s->next(evt));
	}
	return result;
}

bool MultiState::is_final() const {
	for (const State* s : _states) {
		if (s->is_final()) {
			return true;
		}
	}
	return false;
}

const State& MultiState::find_final() const {
	assert(is_final());
	for (const State* s : _states) {
		if (s->is_final()) {
			return *s;
		}
	}

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

Observer::Observer(std::vector<std::unique_ptr<State>> states) : _states(std::move(states)) {
	// check for initial states
	if (std::count_if(_states.begin(), _states.end(), [](const std::unique_ptr<State>& s){ return s->is_initial(); }) == 0) {
		throw std::logic_error("Observers must have an initial state");
	}

	// make initial state
	std::vector<const State*> init;
	for (const auto& s : _states) {
		if (s->is_initial()) {
			init.push_back(s.get());
		}
	}
	_init = MultiState(std::move(init));
}

