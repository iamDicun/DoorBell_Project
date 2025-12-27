#pragma once

#include <queue>

enum EventType {
    EVENT_NONE = 0,
    EVENT_BUTTON_SHORT_PRESS,
    EVENT_BUTTON_LONG_PRESS,
    EVENT_BUTTON_LONG_RELEASE
};

struct Event {
    EventType type;
    unsigned long timestamp;
};

extern std::queue<Event> eventQueue;

void eventManagerInit();
void eventManagerLoop();
Event eventPop();
bool eventAvailable();
