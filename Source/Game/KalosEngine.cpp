#include "KalosEngine.h"

#include <chrono>

#include <GLFW/glfw3.h>

#include <RendererCore/Tests/RenderTestHandler.h>

KalosEngine::KalosEngine(const std::vector<std::string> &launchArgs, RendererBackend rendererBackendType, uint32_t engineUpdateFrequencyCap)
{
	this->launchArgs = launchArgs;
	this->rendererBackendType = rendererBackendType;
	this->updateFrequencyCap = engineUpdateFrequencyCap;

	lastUpdateTime = getTime();

	engineIsRunning = true;
	
	mainWindow = std::unique_ptr<Window>(new Window(rendererBackendType));
	mainWindow->initWindow(0, 0, APP_NAME);

	mainWindowSize = glm::uvec2(mainWindow->getWidth(), mainWindow->getHeight());

	RendererAllocInfo renderAlloc = {};
	renderAlloc.backend = rendererBackendType;
	renderAlloc.launchArgs = launchArgs;
	renderAlloc.mainWindow = mainWindow.get();

	renderer = std::unique_ptr<Renderer>(Renderer::allocateRenderer(renderAlloc));

	doingRenderingTest = true;

	RendererTest currentRenderingTest;

	if (std::find(launchArgs.begin(), launchArgs.end(), "-triangle_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_TRIANGLE;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-vertex_index_buffer_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_VERTEX_INDEX_BUFFER;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-push_constants_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_PUSH_CONSTANTS;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-cube_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_CUBE;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-compute_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_COMPUTE;
	else
		doingRenderingTest = false;

	if (doingRenderingTest)
		renderTestHandler = std::unique_ptr<RenderTestHandler>(new RenderTestHandler(renderer.get(), mainWindow.get(), currentRenderingTest));
}

KalosEngine::~KalosEngine()
{
	renderer->waitForDeviceIdle();

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

	glm::uvec2 lastMainWindowSize = mainWindowSize;
	mainWindowSize = glm::uvec2(mainWindow->getWidth(), mainWindow->getHeight());

	if (mainWindowSize != lastMainWindowSize)
	{
		renderer->recreateSwapchain(mainWindow.get());

		/*
		if (doingRenderingTest)
		{
			switch (currentRenderingTest)
			{
				case RENDERER_TEST_TRIANGLE:
					triangleTest->gfxGraph->resizeNamedSize("swapchain", mainWindowSize);

					renderer->setSwapchainTexture(mainWindow.get(), triangleTest->gfxGraph->getRenderGraphOutputTextureView(), triangleTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
					break;
				case RENDERER_TEST_VERTEX_INDEX_BUFFER:
					vertexIndexBufferTest->gfxGraph->resizeNamedSize("swapchain", mainWindowSize);

					renderer->setSwapchainTexture(mainWindow.get(), vertexIndexBufferTest->gfxGraph->getRenderGraphOutputTextureView(), vertexIndexBufferTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
					break;
				case RENDERER_TEST_PUSH_CONSTANTS:
					pushConstantsTest->gfxGraph->resizeNamedSize("swapchain", mainWindowSize);

					renderer->setSwapchainTexture(mainWindow.get(), pushConstantsTest->gfxGraph->getRenderGraphOutputTextureView(), pushConstantsTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
					break;
				case RENDERER_TEST_CUBE:
					cubeTest->gfxGraph->resizeNamedSize("swapchain", mainWindowSize);

					renderer->setSwapchainTexture(mainWindow.get(), cubeTest->gfxGraph->getRenderGraphOutputTextureView(), cubeTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
					break;
				case RENDERER_TEST_COMPUTE:
					computeTest->gfxGraph->resizeNamedSize("swapchain", mainWindowSize);

					renderer->setSwapchainTexture(mainWindow.get(), computeTest->gfxGraph->getRenderGraphOutputTextureView(), computeTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
					break;
			}
		}
		*/
	}

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
	if (doingRenderingTest)
	{
		renderTestHandler->render();
	}
	else if (!gameStates.empty())
	{
		gameStates.back()->render();

		renderer->waitForQueueIdle(QUEUE_TYPE_GRAPHICS);

		renderer->presentToSwapchain(mainWindow.get(), {});
	}
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