
#include "Game/EventHandler.h"

EventHandler* EventHandler::eventHandlerInstance;

EventHandler::EventHandler ()
{
	std::unique_lock<std::mutex> lock(eventObservers_mutex);
	for (int i = 0; i < EVENT_MAX_ENUM; i ++)
	{
		eventObservers[static_cast<EventType> (i)] = std::vector<std::pair<void*, void*> > ();
	}
}

EventHandler::~EventHandler ()
{

}

EventHandler *EventHandler::instance ()
{
	return eventHandlerInstance;
}

void EventHandler::setInstance (EventHandler* inst)
{
	eventHandlerInstance = inst;
}
