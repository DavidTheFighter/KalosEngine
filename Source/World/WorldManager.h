#ifndef WORLD_WORLDMANAGER_H_
#define WORLD_WORLDMANAGER_H_

#include <common.h>

typedef struct
{
	uint64_t heightmapDataFilePosition;
	uint64_t objectDataFilePosition;
} WorldInfoLookupEntry;

typedef struct
{
	std::string uniqueName;
	bool hasTerrain;
	uint32_t terrainSizeX;
	uint32_t terrainSizeY;
	int32_t terrainOffsetX;
	int32_t terrainOffsetY;

	std::map<sivec2, WorldInfoLookupEntry> dataLookupTable;
} WorldInfo;

class WorldManager
{
public:
	WorldManager();
	virtual ~WorldManager();

	void loadWorld(const std::string &file);
	void unloadWorld(const std::string &worldUniqueName);

private:

};

#endif /* WORLD_WORLDMANAGER_H_ */