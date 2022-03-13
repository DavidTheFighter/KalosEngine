/*

Recognized launch args:

-force_vulkan
-force_d3d12

-enable_vulkan_layers
-enable_d3d12_debug
-enable_d3d12_hw_debug

-cube_test

*/

#include <iostream>
#include <thread>

#include <common.h>

#include <GLFW/glfw3.h>
#include <assimp/version.h>
#include <lodepng.h>

#include <Game/KalosEngine.h>
#include <Game/GameStateTitleScreen.h>
#include <Game/GameStateInWorld.h>

#include <RendererCore/Renderer.h>
#include <Resources/FileLoader.h>
#include <Resources/ResourceImporter.h>

#include <World/WorldManager.h>


int main(int argc, char * argv[]);

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
		std::ofstream file("A:/Programming/KalosEngineDev-win/GameData/worlds/testworld.kew", std::ios::out | std::ios::binary);

		if (!file.is_open())
		{
			return 0;
		}

		uint16_t fileVersion = 0;
		std::string uniqueName = "testworld";
		uint32_t uniqueNameStrLen = uniqueName.size();
		uint8_t hasTerrain = 1;
		uint32_t terrainSizeX = 4;
		uint32_t terrainSizeY = 4;
		int32_t terrainOffsetX = 0;
		int32_t terrainOffsetY = 0;

		// HEADER
		file.write("KEW|", 4);
		file.write(reinterpret_cast<const char*>(&fileVersion), sizeof(fileVersion));

		// WORLD INFO
		file.write(reinterpret_cast<const char*>(&uniqueNameStrLen), sizeof(uniqueNameStrLen));
		file.write(uniqueName.c_str(), uniqueNameStrLen);
		file.write(reinterpret_cast<const char*>(&hasTerrain), sizeof(hasTerrain));
		file.write(reinterpret_cast<const char*>(&terrainSizeX), sizeof(terrainSizeX));
		file.write(reinterpret_cast<const char*>(&terrainSizeY), sizeof(terrainSizeY));
		file.write(reinterpret_cast<const char*>(&terrainOffsetX), sizeof(terrainOffsetX));
		file.write(reinterpret_cast<const char*>(&terrainOffsetY), sizeof(terrainOffsetY));

		uint64_t zeroValue = 0;
		std::map<sivec2, std::streampos> objectDataFilePos;

		// LOOKUP TABLE
		for (int64_t x = terrainOffsetX; x < int64_t(terrainSizeX) - terrainOffsetX; x++)
		{
			for (int64_t y = terrainOffsetY; y < int64_t(terrainSizeY) - terrainOffsetY; y++)
			{
				file.write(reinterpret_cast<const char*>(&zeroValue), sizeof(zeroValue));
				objectDataFilePos[{int32_t(x), int32_t(y)}] = file.tellp();
				file.write(reinterpret_cast<const char*>(&zeroValue), sizeof(zeroValue));
			}
		}

		// HEIGHTMAP DATA

		// STATIC OBJECT DATA
		for (int64_t x = terrainOffsetX; x < int64_t(terrainSizeX) - terrainOffsetX; x++)
		{
			for (int64_t y = terrainOffsetY; y < int64_t(terrainSizeY) - terrainOffsetY; y++)
			{
				// Write the lookup table position
				std::streampos currentPos = file.tellp();
				uint64_t currentPos_u64 = uint64_t(currentPos);
				file.seekp(objectDataFilePos[{int32_t(x), int32_t(y)}]);
				file.write(reinterpret_cast<const char *>(&currentPos_u64), sizeof(currentPos_u64));
				file.seekp(currentPos);
			
				AABB chunkAABB = {{0, 0, 0, 0}, {256.0f, 256.0f, 256.0f, 0}};
				file.write(reinterpret_cast<const char *>(&chunkAABB), sizeof(chunkAABB));

				// Generate a test octree with random shit in it
				Octree<StaticObjectEntry> testOctree;
				testOctree.boundingBox = chunkAABB;

				std::vector<StaticObjectEntry> items;
				for (uint32_t i = 0; i < 4096; i++)
				{
					StaticObjectEntry entry = {};
					entry.objectUUID = rand();
					entry.meshID = rand();
					entry.materialID = rand();
					entry.position = {(rand() / float(RAND_MAX)) * 256.0f, (rand() / float(RAND_MAX)) * 256.0f, (rand() / float(RAND_MAX)) * 256.0f};
					entry.scale = 1.0f;
					entry.orientation = {0, 0, 0, 1};
					entry.boundingSphereRadius = rand() % 64;
					entry.bitmask = 0;

					items.push_back(entry);
				}

				insertItemsIntoOctree(&testOctree, items);

				std::vector<Octree<StaticObjectEntry> *> octreeList;
				std::map<Octree<StaticObjectEntry> *, uint32_t> octreeList_map;

				std::function<void(Octree<StaticObjectEntry> *node)> recursiveAddOctreeToList = [&octreeList, &octreeList_map, &recursiveAddOctreeToList] (Octree<StaticObjectEntry> *node) -> void {
					octreeList.push_back(node);
					octreeList_map[node] = octreeList.size() - 1;

					for (int child = 0; child < 8; child++)
						if (node->children[child] != nullptr)
							recursiveAddOctreeToList(node->children[child]);
				};

				recursiveAddOctreeToList(&testOctree);

				uint32_t octreeArrayCount = octreeList.size();
				file.write(reinterpret_cast<const char *>(&octreeArrayCount), sizeof(octreeArrayCount));

				for (size_t n = 0; n < octreeList.size(); n++)
				{
					Octree<StaticObjectEntry> *node = octreeList[n];
					uint32_t parentNodeIndex = node->parent == nullptr ? 0xFFFFFFFF : octreeList_map[node->parent];
					file.write(reinterpret_cast<const char *>(&parentNodeIndex), sizeof(parentNodeIndex));

					uint32_t childNodeIndex[8];

					for (int child = 0; child < 8; child++)
					{
						childNodeIndex[child] = node->children[child] == nullptr ? 0xFFFFFFFF : octreeList_map[node->children[child]];
						file.write(reinterpret_cast<const char *>(&childNodeIndex[child]), sizeof(childNodeIndex[0]));
					}

					file.write(reinterpret_cast<const char *>(&node->boundingBox), sizeof(node->boundingBox));
					
					uint32_t itemCount = node->items.size();
					file.write(reinterpret_cast<const char *>(&itemCount), sizeof(itemCount));
					
					if (itemCount > 0)
						file.write(reinterpret_cast<const char *>(node->items.data()), itemCount * sizeof(node->items[0]));
				}
			}
		}

		file.close();
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

	//launchArgs.push_back("-triangle_test");
	//launchArgs.push_back("-vertex_index_buffer_test");
	//launchArgs.push_back("-push_constants_test");
	//launchArgs.push_back("-cube_test");
	//launchArgs.push_back("-compute_test");
	launchArgs.push_back("-msaa_test");

	Log::setInstance(new Log());
	JobSystem::setInstance(new JobSystem(16));

	printEnvironment(launchArgs);
	
	//JobSystem::get()->test();

	std::string workingDir = std::string(getenv("DEV_WORKING_DIR")) + "/";

	Log::get()->info("Current working directory: {}", workingDir);

	// Initialize the singletons
	FileLoader::setInstance(new FileLoader());
	FileLoader::instance()->setWorkingDir(workingDir);
	ResourceImporter::setInstance(new ResourceImporter());

	RendererBackend rendererBackend = Renderer::chooseRendererBackend(launchArgs);

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
	engine->worldManager->loadWorld("GameData/worlds/testworld.kew");
	engine->worldManager->setActiveWorld("testworld");
	
	std::unique_ptr<GameStateInWorld> inWorldState(new GameStateInWorld(engine));
	std::unique_ptr<GameStateTitleScreen> titleScreenState(new GameStateTitleScreen(engine, inWorldState.get()));

	engine->pushState(titleScreenState.get());

	Log::get()->info("Completed startup");

	do
	{
		engine->handleEvents();
		engine->update();
		engine->render();
	} while (engine->isRunning());

	Log::get()->info("Beginning shutdown");

	// Delete the singletons
	delete FileLoader::instance();
	delete ResourceImporter::instance();

	delete engine;

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

	Log::get()->info("Completed graceful shutdown");

	delete Log::getInstance();
	delete JobSystem::get();

	//system("pause");

	return 0;
}

void printEnvironment(std::vector<std::string> launchArgs)
{
	std::string launchArgsStr = "";

	for (size_t i = 0; i < launchArgs.size(); i++)
		launchArgsStr += launchArgs[i] + (i < launchArgs.size() - 1 ? ", " : "");

	Log::get()->info("Starting {} {}.{}.{} w/ launch args: {}", ENGINE_NAME, ENGINE_VERSION_MAJOR, ENGINE_VERSION_MINOR, ENGINE_VERSION_REVISION, launchArgsStr);

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