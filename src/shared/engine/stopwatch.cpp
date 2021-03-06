#include "stopwatch.hpp"

#include "shared/engine/logging.hpp"

static logging::Logger logger("default");

Stopwatch::Stopwatch(size_t size) : _clocks(size) {
	auto now = getCurrentTime();
	for (EntryType &entry : _clocks) {
		entry.dur = 0;
		entry.start = now;
		entry.rel = 0.0;
	}
	_clocks[size - 1].rel = 1.0;
}

void Stopwatch::start(uint id) {
    auto now = getCurrentTime();
	if (!_stack.empty()) {
		EntryType &old_entry = _clocks[_stack.top()];
		old_entry.dur += now - old_entry.start;
		_total += now - old_entry.start;
	}
	EntryType &entry = _clocks[id];
	entry.start = now;
	_stack.push(id);
}

void Stopwatch::stop(uint id) {
	if (_stack.empty()) {
		LOG_DEBUG(logger) << "No clock to stop";
		return;
	}
	if (id == (uint) -1) {
		LOG_DEBUG(logger) << "Stopped clock " << _stack.top()
				<< " without explicit id given";
	} else if (id != _stack.top()) {
		LOG_DEBUG(logger) << "Stopped clock " << _stack.top()
				<< " but " << id << " given";
	}
	auto now = getCurrentTime();
	EntryType &entry = _clocks[_stack.top()];
	entry.dur += now - entry.start;
	_total += now - entry.start;
	_stack.pop();
	if (!_stack.empty()) {
		EntryType &old_entry = _clocks[_stack.top()];
		old_entry.start = now;
	}
}

Time Stopwatch::get(uint id) {
	return _clocks[id].dur;
}

Time Stopwatch::getTotal() {
	return _total;
}

float Stopwatch::getRel(uint id) {
	return _clocks[id].rel;
}

void Stopwatch::save() {
	while (!_stack.empty()) {
		LOG_DEBUG(logger) << "stopped clock " << _stack.top() << " unnessessarily";
		stop();
	}

	for (auto &clock: _clocks) {
		clock.rel = (float) clock.dur / _total;
		clock.dur = 0;
	}

	_total = 0;
}
