/*
 * MIT License
 * 
 * Copyright (c) 2017 David Allen
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * EventHandler.h
 * 
 * Created on: Sep 30, 2017
 *     Author: david
 */

#ifndef GAME_EVENTS_EVENTHANDLER_H_
#define GAME_EVENTS_EVENTHANDLER_H_

#include <common.h>
#include <Game/EventTypes.h>

class EventHandler
{
	public:
		EventHandler ();
		virtual ~EventHandler ();

		template<typename EventCallbackFunc>
		/*
		 * Registers a function as an observer of a particular event. Note that if a single
		 * function is going to registered multiple times, then each usrPtr given MUST be
		 * different, to make sure the removeObserver() function removes the correct observer.
		 * An example is a window resize function. Because an observer function must be static,
		 * one would register it multiple times for each window. To distinguish between each
		 * separate observer, the "usrPtr" parameter is used, and therefore must be different
		 * for each time an instance is registered.
		 */
		void registerObserver (EventType eventType, EventCallbackFunc callback, void *usrPtr)
		{
			std::unique_lock<std::mutex> lock(eventObservers_mutex);
			eventObservers[eventType].push_back(std::make_pair(reinterpret_cast<void*>(callback), usrPtr));
		}

		template<typename EventCallbackFunc>
		/*
		 * Removes an observer function. Note that the usrPtr MUST be the same value as the
		 * one passed to registerObserver() when that observer was registered. This is used
		 * to distinguish multiple observers that all point to a single function.
		 */
		void removeObserver (EventType eventType, EventCallbackFunc callback, void *usrPtr)
		{
			std::unique_lock<std::mutex> lock(eventObservers_mutex);
			std::vector<std::pair<void*, void*> > &observerList = eventObservers[eventType];

			auto it = std::find(observerList.begin(), observerList.end(), std::make_pair(reinterpret_cast<void*>(callback), usrPtr));

			if (it != observerList.end())
			{
				observerList.erase(it);
			}
		}

		template<typename EventData>
		void triggerEvent (EventType eventType, const EventData& eventData)
		{
			std::unique_lock<std::mutex> lock(eventObservers_mutex);

			std::vector<std::pair<void*, void*> > observerList = eventObservers[eventType];

			for (size_t i = 0; i < observerList.size(); i ++)
			{
				(reinterpret_cast<void (*) (const EventData&, void*)>(observerList[i].first))(eventData, observerList[i].second);
			}
		}

		static EventHandler *instance ();
		static void setInstance (EventHandler* inst);

	private:

		std::mutex eventObservers_mutex; // Controls access of member "eventObservers"
		std::map<EventType, std::vector<std::pair<void*, void*> > > eventObservers; // Each element of the observer list is a pair, 1st=callback, 2nd=usr ptr

		static EventHandler *eventHandlerInstance;
};

#endif /* GAME_EVENTS_EVENTHANDLER_H_ */
