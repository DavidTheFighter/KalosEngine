#include "World/WorldManager.h"

#include <Resources/FileLoader.h>

WorldManager::WorldManager()
{
	DEBUG_ASSERT(sizeof(StaticObjectEntry) == 64);

	activeWorld = nullptr;
}

WorldManager::~WorldManager()
{

}

void WorldManager::loadWorld(const std::string &fileName)
{
	std::vector<char> file = FileLoader::instance()->readFileBuffer(fileName);

	if (file.size() < 4)
		return;

	// HEADER

	if (!datacmp(&file[0], "KEW|", 4))
	{
		Log::get()->error("Failed to load {} as a world file, header \"{}\" is invalid, expected \"KEW|\"", fileName, std::string(&file[0], 4));
		return;
	}

	uint64_t offset = 4;
	uint16_t fileVersion;
	
	seqread(&fileVersion, file.data(), sizeof(fileVersion), offset);

	if (fileVersion != 0)
	{
		Log::get()->error("Failed to load {} as a world file, file has outdated version {}", fileVersion);
		return;
	}

	// WORLD INFO
	WorldInfo *worldInfoPtr = new WorldInfo();
	WorldInfo &worldInfo = *worldInfoPtr;

	seqreadstr(worldInfo.uniqueName, file.data(), offset);
	seqread(&worldInfo.hasTerrain, file.data(), sizeof(worldInfo.hasTerrain), offset);
	seqread(&worldInfo.terrainSizeX, file.data(), sizeof(worldInfo.terrainSizeX), offset);
	seqread(&worldInfo.terrainSizeY, file.data(), sizeof(worldInfo.terrainSizeY), offset);
	seqread(&worldInfo.terrainOffsetX, file.data(), sizeof(worldInfo.terrainOffsetX), offset);
	seqread(&worldInfo.terrainOffsetY, file.data(), sizeof(worldInfo.terrainOffsetY), offset);

	// LOOKUP TABLE
	for (int64_t x = worldInfo.terrainOffsetX; x < int64_t(worldInfo.terrainSizeX) - worldInfo.terrainOffsetX; x++)
	{
		for (int64_t y = worldInfo.terrainOffsetY; y < int64_t(worldInfo.terrainSizeY) - worldInfo.terrainOffsetY; y++)
		{
			WorldInfoLookupEntry entry = {};
			seqread(&entry.heightmapDataFilePosition, file.data(), sizeof(entry.heightmapDataFilePosition), offset);
			seqread(&entry.objectDataFilePosition, file.data(), sizeof(entry.objectDataFilePosition), offset);

			worldInfo.dataLookupTable[{int32_t(x), int32_t(y)}] = entry;
		}
	}

	// HEIGHTMAP DATA

	// STATIC OBJECT DATA
	for (int64_t x = worldInfo.terrainOffsetX; x < int64_t(worldInfo.terrainSizeX) - worldInfo.terrainOffsetX; x++)
	{
		for (int64_t y = worldInfo.terrainOffsetY; y < int64_t(worldInfo.terrainSizeY) - worldInfo.terrainOffsetY; y++)
		{
			offset = worldInfo.dataLookupTable[{int32_t(x), int32_t(y)}].objectDataFilePosition;

			WorldChunkStaticObjectData data = {};
			seqread(&data.chunkAABB, file.data(), sizeof(data.chunkAABB), offset);

			uint32_t octreeCount = 0;
			seqread(&octreeCount, file.data(), sizeof(octreeCount), offset);

			Octree<StaticObjectEntry> *octrees = new Octree<StaticObjectEntry>[octreeCount];

			for (uint32_t n = 0; n < octreeCount; n++)
			{
				Octree<StaticObjectEntry> *node = &octrees[n];
				uint32_t parentNodeIndex, childNodeIndex[8];

				seqread(&parentNodeIndex, file.data(), sizeof(parentNodeIndex), offset);
				node->parent = parentNodeIndex == 0xFFFFFFFF ? nullptr : &octrees[parentNodeIndex];

				for (int c = 0; c < 8; c++)
				{
					seqread(&childNodeIndex[c], file.data(), sizeof(childNodeIndex[c]), offset);
					node->children[c] = childNodeIndex[c] == 0xFFFFFFFF ? nullptr : &octrees[childNodeIndex[c]];
				}

				seqread(&node->boundingBox, file.data(), sizeof(node->boundingBox), offset);

				uint32_t itemCount;
				seqread(&itemCount, file.data(), sizeof(itemCount), offset);

				if (itemCount > 0)
				{
					node->items.resize(itemCount);
					seqread(node->items.data(), file.data(), itemCount * sizeof(node->items[0]), offset);
				}
			}
		}
	}

	loadedWorlds[worldInfo.uniqueName] = worldInfoPtr;
}

void WorldManager::setActiveWorld(const std::string &worldUniqueName)
{
	activeWorld = getLoadedWorld(worldUniqueName);
}

WorldInfo *WorldManager::getActiveWorld()
{
	return activeWorld;
}

WorldInfo *WorldManager::getLoadedWorld(const std::string &worldUniqueName)
{
	auto worldIt = loadedWorlds.find(worldUniqueName);

	if (worldIt == loadedWorlds.end())
	{
		Log::get()->error("Tried getting world \"{}\" that has not been loaded yet!", worldUniqueName);
		throw std::runtime_error("tried getting world that is not loaded");
	}

	return worldIt->second;
}

void WorldManager::unloadWorld(const std::string &worldUniqueName)
{

}