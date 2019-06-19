#ifndef WORLD_WORLDMANAGER_H_
#define WORLD_WORLDMANAGER_H_

#include <common.h>
#include <Util/SpatialStructures.h>

struct alignas(64) StaticObjectEntry
{
	uint64_t objectUUID;
	uint64_t meshID;
	uint64_t materialID;
	svec3 position;
	float scale;
	svec4 orientation;
	float boundingSphereRadius;
	uint32_t bitmask;

	inline BoundingSphere getBoundingSphere() const
	{
		return {position, boundingSphereRadius * scale};
	}
};

typedef struct
{
	uint64_t heightmapDataFilePosition;
	uint64_t objectDataFilePosition;
} WorldInfoLookupEntry;

struct WorldChunkStaticObjectData
{
	AABB chunkAABB; // Also the AABB of the top level of the octree

	Octree<StaticObjectEntry> *chunkOctree;
};

typedef struct
{
	std::string uniqueName;
	bool hasTerrain;
	uint32_t terrainSizeX;
	uint32_t terrainSizeY;
	int32_t terrainOffsetX;
	int32_t terrainOffsetY;

	std::map<sivec2, WorldInfoLookupEntry> dataLookupTable;

	std::vector<WorldChunkStaticObjectData> staticObjectData; // Arranged by terrain sizes, aka size = terrainSizeX * terrainSizeY, accessed by [x * terrainSizeX + y]
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