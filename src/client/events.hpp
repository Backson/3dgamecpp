#ifndef EVENTS_HPP_
#define EVENTS_HPP_

#include "shared/engine/std_types.hpp"
#include "SDL2/SDL_events.h"

enum class EventType {
	NONE,
	WINDOW_SHOWN,
	WINDOW_HIDDEN,
	WINDOW_EXPOSED,
	WINDOW_MOVED,
	WINDOW_RESIZED,
	WINDOW_SIZE_CHANGED,
	WINDOW_MINIMIZED,
	WINDOW_MAXIMIZED,
	WINDOW_RESTORED,
	WINDOW_ENTER,
	WINDOW_LEAVE,
	WINDOW_FOCUS_GAINED,
	WINDOW_FOCUS_LOST,
	WINDOW_CLOSE,
	KEYBOARD_PRESSED,
	KEYBOARD_RELEASED,
	KEYBOARD_REPEAT,
	MOUSE_MOTION,
	MOUSE_BUTTON_PRESSED,
	MOUSE_BUTTON_RELEASED,
	MOUSE_WHEEL,
	OTHER,
};

struct Event {
	EventType type;
	SDL_Event event;

	bool next();
};

#endif // EVENTS_HPP_
