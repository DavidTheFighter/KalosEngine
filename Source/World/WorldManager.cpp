#include "World/WorldManager.h"

#include <Resources/FileLoader.h>

WorldManager::WorldManager()
{

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

	size_t offset = 4;
	uint16_t fileVersion;
	
	seqread(&fileVersion, file.data(), sizeof(fileVersion), offset);

	if (fileVersion != 0)
	{
		Log::get()->error("Failed to load {} as a world file, file has outdated version {}", fileVersion);
		return;
	}

	// WORLD INFO
	WorldInfo worldInfo = {};

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
}

void WorldManager::unloadWorld(const std::string &worldUniqueName)
{

}