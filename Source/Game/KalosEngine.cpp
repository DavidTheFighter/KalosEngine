#include "KalosEngine.h"

#include <chrono>

#include <GLFW/glfw3.h>

KalosEngine::KalosEngine(const std::vector<std::string> &launchArgs, RendererBackend rendererBackendType, uint32_t engineUpdateFrequencyCap)
{
	this->launchArgs = launchArgs;
	this->rendererBackendType = rendererBackendType;
	this->updateFrequencyCap = engineUpdateFrequencyCap;

	lastUpdateTime = getTime();

	engineIsRunning = true;
	
	mainWindow = std::unique_ptr<Window>(new Window(rendererBackendType));
	mainWindow->initWindow(0, 0, APP_NAME);
}


KalosEngine::~KalosEngine()
{
	while (!gameStates.empty())
	{
		gameStates.back()->destroy();
		gameStates.pop_back();
	}
}

void KalosEngine::setEngineMaxUpdateFrequency(uint32_t engineUpdateFrequencyCap)
{
	updateFrequencyCap = engineUpdateFrequencyCap;
}

void KalosEngine::handleEvents()
{
	mainWindow->pollEvents();

	if (mainWindow->userRequestedClose())
	{
		quit();

		return;
	}

	if (!gameStates.empty())
		gameStates.back()->handleEvents();
}

void KalosEngine::update()
{
	if (true)
	{
		double frameTimeTarget = 1 / double(updateFrequencyCap);

		if (getTime() - lastUpdateTime < frameTimeTarget)
		{
#ifdef __linux__
			usleep(uint32_t(std::max<double>(frameTimeTarget - (getTime() - lastUpdateTime) - 0.001, 0) * 1000000.0));
#elif defined(_WIN32)
			Sleep(DWORD(std::max<double>(frameTimeTarget - (getTime() - lastUpdateTime) - 0.001, 0) * 1000.0));
#endif
			while (getTime() - lastUpdateTime < frameTimeTarget)
			{
				// Busy wait for the rest of the time
			}
		}
	}

	float delta = getTime() - lastUpdateTime;

	if (true)
	{
		static double windowTitleFrametimeUpdateTimer;

		windowTitleFrametimeUpdateTimer += getTime() - lastUpdateTime;

		if (windowTitleFrametimeUpdateTimer > 0.3333)
		{
			windowTitleFrametimeUpdateTimer = 0;

			char windowTitle[256];
			sprintf(windowTitle, "%s (delta-t - %.3f ms, cpu-t - %.3fms)", APP_NAME, delta * 1000.0, lastLoopCPUTime * 1000.0);

			mainWindow->setTitle(windowTitle);
		}
	}

	lastUpdateTime = getTime();

	if (!gameStates.empty())
		gameStates.back()->update(delta);
}

void KalosEngine::render()
{
	if (!gameStates.empty())
		gameStates.back()->render();
}

void KalosEngine::changeState(GameState *state)
{
	if (!gameStates.empty())
	{
		gameStates.back()->destroy();
		gameStates.pop_back();
	}

	gameStates.push_back(state);
	gameStates.back()->init();
}

void KalosEngine::pushState(GameState *state)
{
	if (!gameStates.empty())
	{
		gameStates.back()->pause();
	}

	gameStates.push_back(state);
	gameStates.back()->init();
}

void KalosEngine::popState()
{
	if (!gameStates.empty())
	{
		gameStates.back()->destroy();
		gameStates.pop_back();
	}

	if (!gameStates.empty())
	{
		gameStates.back()->resume();
	}
}

bool KalosEngine::isRunning()
{
	return engineIsRunning;
}

void KalosEngine::quit()
{
	engineIsRunning = false;
}

double KalosEngine::getTime()
{
	if (rendererBackendType == RENDERER_BACKEND_VULKAN || rendererBackendType == RENDERER_BACKEND_D3D12)
		return glfwGetTime();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1000000000.0;
}

GameState::~GameState()
{

}