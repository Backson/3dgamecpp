#include "stopwatch.hpp"

using namespace std::chrono;

Stopwatch::Stopwatch(size_t size) : _clocks(size), _total(dur_type::zero()) {
	auto now = high_resolution_clock::now();
	for (EntryType &entry : _clocks) {
		entry.dur = dur_type::zero();
		entry.start = now;
		entry.rel = 0.0;
	}
	_clocks[size - 1].rel = 1.0;
}

void Stopwatch::start(uint id) {
	auto now = high_resolution_clock::now();
	if (!_stack.empty()) {
		EntryType &old_entry = _clocks[_stack.top()];
		old_entry.dur += now - old_entry.start;
		_total += now - old_entry.start;
	}
	EntryType &entry = _clocks[id];
	entry.start = now;
	_stack.push(id);
}

void Stopwatch::stop() {
	auto now = high_resolution_clock::now();
	EntryType &entry = _clocks[_stack.top()];
	entry.dur += now - entry.start;
	_total += now - entry.start;
	_stack.pop();
	if (!_stack.empty()) {
		EntryType &old_entry = _clocks[_stack.top()];
		old_entry.start = now;
	}
}

auto Stopwatch::get(uint id) -> dur_type {
	return _clocks[id].dur;
}

auto Stopwatch::getTotal() -> dur_type {
	return _total;
}

float Stopwatch::getRel(uint id) {
	return _clocks[id].rel;
}

void Stopwatch::stopAndSave() {
	while (!_stack.empty())
		stop();

	for (auto &clock: _clocks) {
		clock.rel = (double) clock.dur.count() / _total.count();
		clock.dur = dur_type::zero();
	}

	_total = dur_type::zero();
}