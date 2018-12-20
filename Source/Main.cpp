#include <iostream>
#include <thread>

#include <common.h>

#include <GLFW/glfw3.h>
#include <assimp/version.h>
#include <lodepng.h>

#include <Game/EventHandler.h>
#include <Game/KalosEngine.h>

#include <RendererCore/RendererCore.h>
#include <Resources/FileLoader.h>

void printEnvironment(std::vector<std::string> launchArgs);

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	std::vector<std::string> launchArgs;

	for (int i = 0; i < argc; i++)
	{
		launchArgs.push_back(argv[i]);
	}

	if (true)
	{
		launchArgs.push_back("-force_vulkan");
		launchArgs.push_back("-enable_vulkan_layers");
	}
	else
	{
		launchArgs.push_back("-force_d3d12");
		launchArgs.push_back("-enable_d3d12_debug");
		//launchArgs.push_back("-enable_d3d12_hw_debug");
	}

	Log::setInstance(new Log());

	printEnvironment(launchArgs);

	std::string workingDir = std::string(getenv("DEV_WORKING_DIR")) + "/";

	Log::get()->info("Current working directory: {}", workingDir);

	// Initialize the singletons
	EventHandler::setInstance(new EventHandler());
	FileLoader::setInstance(new FileLoader());
	FileLoader::instance()->setWorkingDir(workingDir);

	RendererBackend rendererBackend = RendererCore::chooseRendererBackend(launchArgs);

	switch (rendererBackend)
	{
		case RENDERER_BACKEND_VULKAN:
		case RENDERER_BACKEND_D3D12:
		{
			glfwInit();

			break;
		}
		default:
			break;
	}

	KalosEngine *engine = new KalosEngine(launchArgs, rendererBackend, 60);

	Log::get()->info("Completed startup");

	do
	{
		engine->handleEvents();
		engine->update();
		engine->render();
	} while (engine->isRunning());

	Log::get()->info("Beginning shutdown");

	// Delete the singletons
	delete EventHandler::instance();
	delete FileLoader::instance();

	switch (rendererBackend)
	{
		case RENDERER_BACKEND_VULKAN:
		case RENDERER_BACKEND_D3D12:
		{
			glfwTerminate();

			break;
		}
		default:
			break;
	}

	delete engine;

	Log::get()->info("Completed graceful shutdown");

	delete Log::getInstance();

	return 0;
}

void printEnvironment(std::vector<std::string> launchArgs)
{
	Log::get()->info("Starting {} {}.{}.{} w/ launch args", ENGINE_NAME, ENGINE_VERSION_MAJOR, ENGINE_VERSION_MINOR, ENGINE_VERSION_REVISION);

	std::string osString = "";

#ifdef _WIN32
	osString = "Windows";
#elif __APPLE__
	osString = "Apple?";
#elif __linux__
	osString = "Linux";
#endif

	union
	{
		uint32_t i;
		char c[4];
	} bint = {0x01020304};

	Log::get()->info("OS: {}, Endian: {}, C++ Concurrent Threads: {}", osString, bint.c[0] == 1 ? "Big" : "Little", std::thread::hardware_concurrency());

	int glfwV0, glfwV1, glfwV2;
	glfwGetVersion(&glfwV0, &glfwV1, &glfwV2);

	Log::get()->info("GLFW version: {}.{}.{}, lodepng version: {}, AssImp version: {}.{}.{}", glfwV0, glfwV1, glfwV2, "", aiGetVersionMajor(), aiGetVersionMinor(), aiGetVersionRevision());
}