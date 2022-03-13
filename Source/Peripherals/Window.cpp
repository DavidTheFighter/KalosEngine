/*
 * Window.cpp
 *
 *  Created on: Sep 11, 2017
 *      Author: david
 */

#define GLFW_DOUBLE_PRESS 3

#include "Peripherals/Window.h"

#include <GLFW/glfw3.h>

Window::Window (RendererBackend backend)
{
	windowRendererBackend = backend;
	glfwWindow = nullptr;

	mouseGrabbed = false;

	windowWidth = 1;
	windowHeight = 1;

	cursorX = 0.0;
	cursorY = 0.0;

	memset(keysPressed, 0, sizeof(keysPressed));
	memset(mouseButtonsPressed, 0, sizeof(mouseButtonsPressed));
	memset(mouseButtonPressTime, 0, sizeof(mouseButtonPressTime));
}

Window::~Window ()
{
	
}

/*
 * Inits and creates the window of this object. If windowWidth or windowHeight are equal
 * to zero, then the resolution will be decided by 0.75 * textureResource of the current monitor.
 */
void Window::initWindow (uint32_t windowWidth, uint32_t windowHeight, std::string windowName)
{
	if (windowRendererBackend == RENDERER_BACKEND_VULKAN || windowRendererBackend == RENDERER_BACKEND_D3D12)
	{
		GLFWmonitor* monitor = glfwGetPrimaryMonitor();
		const GLFWvidmode* videomode = glfwGetVideoMode(monitor);

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		glfwWindowHint(GLFW_FOCUSED, 1);
		glfwWindowHint(GLFW_AUTO_ICONIFY, 0);
		glfwWindowHint(GLFW_DECORATED, 1);

		bool testUltrawide = false;

		int glfwWindowWidth = windowWidth == 0 ? int(videomode->width * 0.75f) : (int) windowWidth;
		int glfwWindowHeight = windowHeight == 0 ? int(videomode->height * 0.75f) : (int) windowHeight;

		if (testUltrawide)
		{
			glfwWindowWidth = videomode->width;
			glfwWindowHeight = (int) (float(glfwWindowWidth) * (9 / 21.0f));
		}

		glfwWindow = glfwCreateWindow(glfwWindowWidth, glfwWindowHeight, windowName.c_str(), nullptr, nullptr);

		if (glfwWindow == nullptr)
		{
			Log::get()->error("Failed to create a GLFW window, window = nullptr");

			throw std::runtime_error("glfw error - window creation");
		}

		if (testUltrawide)
		{
			glfwSetWindowSize(glfwWindow, glfwWindowWidth, glfwWindowHeight);
		}

		// Explicitly center the window, mainly because Windows doesn't do it by default
		glfwSetWindowPos(glfwWindow, (videomode->width - glfwWindowWidth) / 2, (videomode->height - glfwWindowHeight) / 2);

		this->windowWidth = (uint32_t) glfwWindowWidth;
		this->windowHeight = (uint32_t) glfwWindowHeight;

		glfwGetCursorPos(glfwWindow, &cursorX, &cursorY);

		glfwSetWindowUserPointer(glfwWindow, this);
		glfwSetWindowSizeCallback(glfwWindow, glfwWindowResizedCallback);
		glfwSetCursorPosCallback(glfwWindow, glfwWindowCursorMoveCallback);
		glfwSetMouseButtonCallback(glfwWindow, glfwWindowMouseButtonCallback);
		glfwSetKeyCallback(glfwWindow, glfwWindowKeyCallback);
		glfwSetCharModsCallback(glfwWindow, glfwWindowTextCallback);
		glfwSetScrollCallback(glfwWindow, glfwWindowMouseScrollCallback);
	}

	windowTitle = windowName;
}

void Window::glfwWindowResizedCallback (GLFWwindow* window, int width, int height)
{
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));

	windowInstance->windowWidth = (uint32_t) width;
	windowInstance->windowHeight = (uint32_t) height;

	for (size_t i = 0; i < windowInstance->windowResizeCallbacks.size(); i++)
		windowInstance->windowResizeCallbacks[i](windowInstance, (uint32_t)width, (uint32_t)height);
}

void Window::glfwWindowCursorMoveCallback (GLFWwindow* window, double newCursorX, double newCursorY)
{
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));

	windowInstance->cursorX = newCursorX;
	windowInstance->cursorY = newCursorY;

	for (size_t i = 0; i < windowInstance->cursorMoveCallbacks.size(); i++)
		windowInstance->cursorMoveCallbacks[i](windowInstance, newCursorX, newCursorY);
}

void Window::glfwWindowMouseButtonCallback (GLFWwindow* window, int button, int action, int mods)
{
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));

	bool doubleClick = false;

	switch (action)
	{
		case GLFW_PRESS:
		{
			if (glfwGetTime() - windowInstance->mouseButtonPressTime[button] < 0.5)
			{
				doubleClick = true;
			}

			windowInstance->mouseButtonsPressed[button] = true;
			windowInstance->mouseButtonPressTime[button] = glfwGetTime();

			break;
		}
		case GLFW_REPEAT:
		{
			windowInstance->mouseButtonsPressed[button] = true;

			break;
		}
		case GLFW_RELEASE:
		{
			windowInstance->mouseButtonsPressed[button] = false;

			break;
		}
		case GLFW_DOUBLE_PRESS:
		{
			windowInstance->mouseButtonsPressed[button] = true;

			break;
		}
	}

	if (doubleClick)
	{
		glfwWindowMouseButtonCallback(window, button, GLFW_DOUBLE_PRESS, mods);
	}

	for (size_t i = 0; i < windowInstance->mouseButtonCallbacks.size(); i++)
		windowInstance->mouseButtonCallbacks[i](windowInstance, (WindowInputMouseButton) button, (WindowInputAction) action, (WindowInputMod) mods);
}

void Window::glfwWindowKeyCallback (GLFWwindow* window, int key, int scancode, int action, int mods)
{
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));

//	printf("window=%p key=%i, scancode=%i, action=%i, mods=%i\n", window, key, scancode, action, mods);

	switch (action)
	{
		case GLFW_PRESS:
		case GLFW_REPEAT:
		{
			windowInstance->keysPressed[key] = 1;

			break;
		}
		case GLFW_RELEASE:
		{
			windowInstance->keysPressed[key] = 0;

			break;
		}
	}

	windowInstance->keyEventStack.push_back(std::make_tuple((WindowInputKeyID) key, (WindowInputAction) action, (WindowInputMod) mods));
}

void Window::glfwWindowTextCallback(GLFWwindow *window, unsigned int codepoint, int mods)
{
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));

	windowInstance->textCodepointStack.push_back((uint32_t) codepoint);

	for (size_t i = 0; i < windowInstance->textCallbacks.size(); i++)
		windowInstance->textCallbacks[i](windowInstance, (uint32_t) codepoint, (WindowInputMod) mods);
}

void Window::glfwWindowMouseScrollCallback (GLFWwindow* window, double xoffset, double yoffset)
{
	Window* windowInstance = static_cast<Window*>(glfwGetWindowUserPointer(window));

	for (size_t i = 0; i < windowInstance->mouseScrollCallbacks.size(); i++)
		windowInstance->mouseScrollCallbacks[i](windowInstance, xoffset, yoffset);
}

void Window::setTitle (std::string title)
{
	switch (windowRendererBackend)
	{
		case RENDERER_BACKEND_VULKAN:
		case RENDERER_BACKEND_D3D12:
		{
			glfwSetWindowTitle(glfwWindow, title.c_str());

			break;
		}
		default:
			break;
	}

	windowTitle = title;
}

void Window::pollEvents ()
{
	textCodepointStack.clear();
	keyEventStack.clear();

	switch (windowRendererBackend)
	{
		case RENDERER_BACKEND_VULKAN:
		case RENDERER_BACKEND_D3D12:
		{
			glfwPollEvents();

			break;
		}
		default:
			break;
	}
}

void Window::setMouseGrabbed (bool grabbed)
{
	if (mouseGrabbed == grabbed)
		return;

	switch (windowRendererBackend)
	{
		case RENDERER_BACKEND_VULKAN:
		case RENDERER_BACKEND_D3D12:
		{
			glfwSetCursorPos(glfwWindow, getWidth() / 2, getHeight() / 2);

			cursorX = getWidth() / 2;
			cursorY = getHeight() / 2;

			if (grabbed)
			{
				glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			}
			else
			{
				glfwSetInputMode(glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			}

			break;
		}
		default:
			break;
	}

	mouseGrabbed = grabbed;
}

void Window::toggleMouseGrabbed ()
{
	setMouseGrabbed(!mouseGrabbed);
}

bool Window::isKeyPressed (int key)
{
	return keysPressed[key] == 1;
}

bool Window::isMouseGrabbed ()
{
	return mouseGrabbed;
}

bool Window::isMouseButtonPressed (int button)
{
	return mouseButtonsPressed[button];
}

std::vector<uint32_t> Window::getInputCodepointStack()
{
	return textCodepointStack;
}

std::vector<std::tuple<WindowInputKeyID, WindowInputAction, WindowInputMod>> Window::getInputKeyEventStack()
{
	return keyEventStack;
}

uint32_t Window::getWidth ()
{
	return windowWidth;
}

uint32_t Window::getHeight ()
{
	return windowHeight;
}

double Window::getCursorX ()
{
	return cursorX;
}

double Window::getCursorY ()
{
	return cursorY;
}

std::string Window::getTitle()
{
	return windowTitle;
}

bool Window::userRequestedClose ()
{
	switch (windowRendererBackend)
	{
		case RENDERER_BACKEND_VULKAN:
		case RENDERER_BACKEND_D3D12:
			return glfwWindowShouldClose(glfwWindow);
		default:
			return true;
	}
}

/*
 * Returns a ptr to the window handle used by the window class. In the case
 * of an OpenGL or Vulkan backend, the handle returned is a GLFWwindow*.
 */
void* Window::getWindowObjectPtr ()
{
	switch (windowRendererBackend)
	{
		case RENDERER_BACKEND_VULKAN:
		case RENDERER_BACKEND_D3D12:
			return glfwWindow;
		default:
			return nullptr;
	}
}

const RendererBackend &Window::getRendererBackend ()
{
	return windowRendererBackend;
}

void Window::addWindowResizeCallback(std::function<void(Window *, uint32_t, uint32_t)> callback)
{
	windowResizeCallbacks.push_back(callback);
}

void Window::addCursorMoveCallback(std::function<void(Window *, float, float)> callback)
{
	cursorMoveCallbacks.push_back(callback);
}

void Window::addMouseButtonCallback(std::function<void(Window *, WindowInputMouseButton, WindowInputAction, WindowInputMod)> callback)
{
	mouseButtonCallbacks.push_back(callback);
}

void Window::addTextCallback(std::function<void(Window *, uint32_t, WindowInputMod)> callback)
{
	textCallbacks.push_back(callback);
}

void Window::addMouseScrollCallback(std::function<void(Window *, float, float)> callback)
{
	mouseScrollCallbacks.push_back(callback);
}
