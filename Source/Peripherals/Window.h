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

		bool userRequestedClose ();

		void* getWindowObjectPtr ();
		const RendererBackend &getRendererBackend();

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

		// An array of each mouse button, and if it's current state (pressed or not pressed)
		bool mouseButtonsPressed[9];

		// An array of the last time a mouse button was pressed, mainly used for double click detection
		double mouseButtonPressTime[9];

		RendererBackend windowRendererBackend;

		static void glfwWindowResizedCallback (GLFWwindow* window, int width, int height);
		static void glfwWindowCursorMoveCallback (GLFWwindow* window, double newCursorX, double newCursorY);
		static void glfwWindowMouseButtonCallback (GLFWwindow* window, int button, int action, int mods);
		static void glfwWindowKeyCallback (GLFWwindow* window, int key, int scancode, int action, int mods);
		static void glfwWindowTextCallback(GLFWwindow *window, unsigned int codepoint, int mods);
		static void glfwWindowMouseScrollCallback (GLFWwindow* window, double xoffset, double yoffset);
};

#endif /* INPUT_WINDOW_H_ */
