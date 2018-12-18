
#ifndef GAME_EVENTS_EVENTTYPES_H_
#define GAME_EVENTS_EVENTTYPES_H_

#include <common.h>

class Window;

typedef enum EventType
{
	EVENT_WINDOW_RESIZE = 0,
	EVENT_CURSOR_MOVE,
	EVENT_MOUSE_BUTTON,
	EVENT_KEY_ACTION,
	EVENT_TEXT_ACTION,
	EVENT_MOUSE_SCROLL,
	EVENT_MAX_ENUM
} EventType;

typedef struct EventWindowResizeData
{
		Window *window;
		uint32_t width;
		uint32_t height;
		uint32_t oldWidth;
		uint32_t oldHeight;
} EventWindowResizeData;

typedef struct EventCursorMoveData
{
		Window *window;
		double cursorX;
		double cursorY;
		double oldCursorX;
		double oldCursorY;
} EventCursorMoveData;

typedef struct EventMouseButtonData
{
		Window *window;
		int button;
		int action;
		int mods;
} EventMouseButtonData;

typedef struct EventKeyActionData
{
		Window *window;
		int key;
		int scancode;
		int action;
		int mods;
} EventKeyActionData;

typedef struct EventTextActionData
{
	Window *window;
	uint32_t codepoint;
	int mods;
} EvenTextActionData;

typedef struct EventMouseScrollData
{
		Window *window;
		double deltaX;
		double deltaY;
} EventMouseScrollData;

typedef void (*EventWindowResizeCallback) (const EventWindowResizeData&, void*);
typedef void (*EventCursorMoveCallback) (const EventCursorMoveData&, void*);
typedef void (*EventMouseButtonCallback) (const EventMouseButtonData&, void*);
typedef void (*EventKeyActionCallback) (const EventKeyActionData&, void*);
typedef void (*EventMouseScrollCallback) (const EventMouseScrollData&, void*);
typedef void (*EventTextActionCallback) (const EventTextActionData&, void*);

#endif /* GAME_EVENTS_EVENTTYPES_H_ */
