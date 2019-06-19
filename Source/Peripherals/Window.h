/*
 * Window.h
 *
 *  Created on: Sep 11, 2017
 *      Author: david
 */

#ifndef INPUT_WINDOW_H_
#define INPUT_WINDOW_H_

#include <common.h>
#include <RendererCore/RendererBackends.h>

enum WindowInputAction
{
	WINDOW_INPUT_ACTION_RELEASE = 0,
	WINDOW_INPUT_ACTION_PRESS = 1,
	WINDOW_INPUT_ACTION_REPEAT = 2,
	WINDOW_INPUT_ACTION_DOUBLE_CLICK = 3
};

enum WindowInputMouseButton
{
	WINDOW_INPUT_MOUSE_BUTTON_1 = 0,
	WINDOW_INPUT_MOUSE_BUTTON_2 = 1,
	WINDOW_INPUT_MOUSE_BUTTON_3 = 2,
	WINDOW_INPUT_MOUSE_BUTTON_4 = 3,
	WINDOW_INPUT_MOUSE_BUTTON_5 = 4,
	WINDOW_INPUT_MOUSE_BUTTON_6 = 5,
	WINDOW_INPUT_MOUSE_BUTTON_7 = 6,
	WINDOW_INPUT_MOUSE_BUTTON_8 = 7,

	WINDOW_INPUT_MOUSE_BUTTON_LEFT = WINDOW_INPUT_MOUSE_BUTTON_1,
	WINDOW_INPUT_MOUSE_BUTTON_RIGHT = WINDOW_INPUT_MOUSE_BUTTON_2,
	WINDOW_INPUT_MOUSE_BUTTON_MIDDLE = WINDOW_INPUT_MOUSE_BUTTON_3
};

enum WindowInputMod
{
	WINDOW_INPUT_MOD_SHIFT = 0x0001,
	WINDOW_INPUT_MOD_CTRL = 0x0002,
	WINDOW_INPUT_MOD_ALT = 0x0004,
	WINDOW_INPUT_MOD_SUPER = 0x0008,
	WINDOW_INPUT_MOD_CAPS_LOCK = 0x0010,
	WINDOW_INPUT_MOD_NUM_LOCK = 0x0020
};

enum WindowInputKeyID
{
	WINDOW_INPUT_KEY_SPACE = 32,
	WINDOW_INPUT_KEY_APOSTROPHE = 39,
	WINDOW_INPUT_KEY_COMMA = 44,
	WINDOW_INPUT_KEY_MINUS = 45,
	WINDOW_INPUT_KEY_PERIOD = 46,
	WINDOW_INPUT_KEY_SLASH = 47,
	WINDOW_INPUT_KEY_0 = 48,
	WINDOW_INPUT_KEY_1 = 49,
	WINDOW_INPUT_KEY_2 = 50,
	WINDOW_INPUT_KEY_3 = 51,
	WINDOW_INPUT_KEY_4 = 52,
	WINDOW_INPUT_KEY_5 = 53,
	WINDOW_INPUT_KEY_6 = 54,
	WINDOW_INPUT_KEY_7 = 55,
	WINDOW_INPUT_KEY_8 = 56,
	WINDOW_INPUT_KEY_9 = 57,
	WINDOW_INPUT_KEY_SEMICOLON = 59,
	WINDOW_INPUT_KEY_EQUAL = 61,
	WINDOW_INPUT_KEY_A = 65,
	WINDOW_INPUT_KEY_B = 66,
	WINDOW_INPUT_KEY_C = 67,
	WINDOW_INPUT_KEY_D = 68,
	WINDOW_INPUT_KEY_E = 69,
	WINDOW_INPUT_KEY_F = 70,
	WINDOW_INPUT_KEY_G = 71,
	WINDOW_INPUT_KEY_H = 72,
	WINDOW_INPUT_KEY_I = 73,
	WINDOW_INPUT_KEY_J = 74,
	WINDOW_INPUT_KEY_K = 75,
	WINDOW_INPUT_KEY_L = 76,
	WINDOW_INPUT_KEY_M = 77,
	WINDOW_INPUT_KEY_N = 78,
	WINDOW_INPUT_KEY_O = 79,
	WINDOW_INPUT_KEY_P = 80,
	WINDOW_INPUT_KEY_Q = 81,
	WINDOW_INPUT_KEY_R = 82,
	WINDOW_INPUT_KEY_S = 83,
	WINDOW_INPUT_KEY_T = 84,
	WINDOW_INPUT_KEY_U = 85,
	WINDOW_INPUT_KEY_V = 86,
	WINDOW_INPUT_KEY_W = 87,
	WINDOW_INPUT_KEY_X = 88,
	WINDOW_INPUT_KEY_Y = 89,
	WINDOW_INPUT_KEY_Z = 90,
	WINDOW_INPUT_KEY_LEFT_BRACKET = 91,
	WINDOW_INPUT_KEY_BACKSLASH = 92,
	WINDOW_INPUT_KEY_RIGHT_BRACKET = 93,
	WINDOW_INPUT_KEY_GRAVE_ACCENT = 96,

	WINDOW_INPUT_KEY_ESCAPE = 256,
	WINDOW_INPUT_KEY_ENTER = 257,
	WINDOW_INPUT_KEY_TAB = 258,
	WINDOW_INPUT_KEY_BACKSPACE = 259,
	WINDOW_INPUT_KEY_INSERT = 260,
	WINDOW_INPUT_KEY_DELETE = 261,
	WINDOW_INPUT_KEY_RIGHT = 262,
	WINDOW_INPUT_KEY_LEFT = 263,
	WINDOW_INPUT_KEY_DOWN = 264,
	WINDOW_INPUT_KEY_UP = 265,
	WINDOW_INPUT_KEY_PAGE_UP = 266,
	WINDOW_INPUT_KEY_PAGE_DOWN = 267,
	WINDOW_INPUT_KEY_HOME = 268,
	WINDOW_INPUT_KEY_END = 269,
	WINDOW_INPUT_KEY_CAPS_LOCK = 280,
	WINDOW_INPUT_KEY_SCROLL_LOCK = 281,
	WINDOW_INPUT_KEY_NUM_LOCK = 282,
	WINDOW_INPUT_KEY_PRINT_SCREEN = 283,
	WINDOW_INPUT_KEY_PAUSE = 284,
	WINDOW_INPUT_KEY_F1 = 290,
	WINDOW_INPUT_KEY_F2 = 291,
	WINDOW_INPUT_KEY_F3 = 292,
	WINDOW_INPUT_KEY_F4 = 293,
	WINDOW_INPUT_KEY_F5 = 294,
	WINDOW_INPUT_KEY_F6 = 295,
	WINDOW_INPUT_KEY_F7 = 296,
	WINDOW_INPUT_KEY_F8 = 297,
	WINDOW_INPUT_KEY_F9 = 298,
	WINDOW_INPUT_KEY_F10 = 299,
	WINDOW_INPUT_KEY_F11 = 300,
	WINDOW_INPUT_KEY_F12 = 301,
	WINDOW_INPUT_KEY_F13 = 302,
	WINDOW_INPUT_KEY_F14 = 303,
	WINDOW_INPUT_KEY_F15 = 304,
	WINDOW_INPUT_KEY_F16 = 305,
	WINDOW_INPUT_KEY_F17 = 306,
	WINDOW_INPUT_KEY_F18 = 307,
	WINDOW_INPUT_KEY_F19 = 308,
	WINDOW_INPUT_KEY_F20 = 309,
	WINDOW_INPUT_KEY_F21 = 310,
	WINDOW_INPUT_KEY_F22 = 311,
	WINDOW_INPUT_KEY_F23 = 312,
	WINDOW_INPUT_KEY_F24 = 313,
	WINDOW_INPUT_KEY_F25 = 314,
	WINDOW_INPUT_KEY_KP_0 = 320,
	WINDOW_INPUT_KEY_KP_1 = 321,
	WINDOW_INPUT_KEY_KP_2 = 322,
	WINDOW_INPUT_KEY_KP_3 = 323,
	WINDOW_INPUT_KEY_KP_4 = 324,
	WINDOW_INPUT_KEY_KP_5 = 325,
	WINDOW_INPUT_KEY_KP_6 = 326,
	WINDOW_INPUT_KEY_KP_7 = 327,
	WINDOW_INPUT_KEY_KP_8 = 328,
	WINDOW_INPUT_KEY_KP_9 = 329,
	WINDOW_INPUT_KEY_KP_DECIMAL = 330,
	WINDOW_INPUT_KEY_KP_DIVIDE = 331,
	WINDOW_INPUT_KEY_KP_MULTIPLY = 332,
	WINDOW_INPUT_KEY_KP_SUBTRACT = 333,
	WINDOW_INPUT_KEY_KP_ADD = 334,
	WINDOW_INPUT_KEY_KP_ENTER = 335,
	WINDOW_INPUT_KEY_KP_EQUAL = 336,
	WINDOW_INPUT_KEY_LEFT_SHIFT = 340,
	WINDOW_INPUT_KEY_LEFT_CONTROL = 341,
	WINDOW_INPUT_KEY_LEFT_ALT = 342,
	WINDOW_INPUT_KEY_LEFT_SUPER = 343,
	WINDOW_INPUT_KEY_RIGHT_SHIFT = 344,
	WINDOW_INPUT_KEY_RIGHT_CONTROL = 345,
	WINDOW_INPUT_KEY_RIGHT_ALT = 346,
	WINDOW_INPUT_KEY_RIGHT_SUPER = 347,
	WINDOW_INPUT_KEY_MENU = 348
};

struct GLFWwindow;

class Window
{
	public:
		Window (RendererBackend backend);
		virtual ~Window ();

		void initWindow (uint32_t windowWidth, uint32_t windowHeight, std::string windowName);
		void pollEvents ();
		void setTitle (std::string title);

		uint32_t getWidth ();
		uint32_t getHeight ();

		double getCursorX();
		double getCursorY();

		std::string getTitle();

		void setMouseGrabbed (bool grabbed);
		void toggleMouseGrabbed ();

		bool isKeyPressed(int key);

		bool isMouseGrabbed();
		bool isMouseButtonPressed (int button);

		std::vector<uint32_t> getInputCodepointStack();
		std::vector<std::tuple<WindowInputKeyID, WindowInputAction, WindowInputMod>> getInputKeyEventStack();

		bool userRequestedClose ();

		void* getWindowObjectPtr ();
		const RendererBackend &getRendererBackend();

		void addWindowResizeCallback(std::function<void(Window *, uint32_t, uint32_t)> callback);
		void addCursorMoveCallback(std::function<void(Window *, float, float)> callback);
		void addMouseButtonCallback(std::function<void(Window *, WindowInputMouseButton, WindowInputAction, WindowInputMod)> callback);
		//void addKeyCallback(std::function<void(Window *, )> callback);
		void addTextCallback(std::function<void(Window *, uint32_t, WindowInputMod)> callback);
		void addMouseScrollCallback(std::function<void(Window *, float, float)> callback);

	private:

		std::string windowTitle;

		bool mouseGrabbed;

		GLFWwindow* glfwWindow;

		uint32_t windowWidth;
		uint32_t windowHeight;

		double cursorX;
		double cursorY;

		// An array of each key, and it's current state. note that it's size doesn't necessarily correlate to the maximum number of keys available
		int keysPressed[400];

		// An array of each mouse button, and if it's current state
		int mouseButtonsPressed[9];

		// An array of the last time a mouse button was pressed, mainly used for double click detection
		double mouseButtonPressTime[9];

		std::vector<uint32_t> textCodepointStack;
		std::vector<std::tuple<WindowInputKeyID, WindowInputAction, WindowInputMod>> keyEventStack;

		RendererBackend windowRendererBackend;

		std::vector<std::function<void(Window *, uint32_t, uint32_t)>> windowResizeCallbacks;
		std::vector<std::function<void(Window *, float, float)>> cursorMoveCallbacks;
		std::vector<std::function<void(Window *, WindowInputMouseButton, WindowInputAction, WindowInputMod)>> mouseButtonCallbacks;
		std::vector<std::function<void(Window *, uint32_t, WindowInputMod)>> textCallbacks;
		std::vector<std::function<void(Window *, float, float)>> mouseScrollCallbacks;

		static void glfwWindowResizedCallback (GLFWwindow* window, int width, int height);
		static void glfwWindowCursorMoveCallback (GLFWwindow* window, double newCursorX, double newCursorY);
		static void glfwWindowMouseButtonCallback (GLFWwindow* window, int button, int action, int mods);
		static void glfwWindowKeyCallback (GLFWwindow* window, int key, int scancode, int action, int mods);
		static void glfwWindowTextCallback(GLFWwindow *window, unsigned int codepoint, int mods);
		static void glfwWindowMouseScrollCallback (GLFWwindow* window, double xoffset, double yoffset);
};

#endif /* INPUT_WINDOW_H_ */
