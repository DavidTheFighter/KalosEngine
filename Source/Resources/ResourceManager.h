#ifndef RESOURCES_RESOURCEMANAGER_H_
#define RESOURCES_RESOURCEMANAGER_H_

#include <common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

struct NonSkinnedVertex
{
	glm::vec3 vertex;
	float tangentHandedness;
	glm::vec4 normal;
	glm::vec4 tangent;
	glm::vec2 uv0;
	glm::vec2 uv1;
};

struct MaterialDefinition
{
	uint64_t pipelineID;

	std::string textureFiles[MATERIAL_MAX_TEXTURE_COUNT];
};

struct MaterialResource
{
	uint64_t materialID;
	uint64_t pipelineID;

	std::string textureFiles[MATERIAL_MAX_TEXTURE_COUNT];

	Texture materialTextures[MATERIAL_MAX_TEXTURE_COUNT];
	TextureView materialTextureViews[MATERIAL_MAX_TEXTURE_COUNT];

	//uint32_t materialTexturesLowestLoadedLevel[MATERIAL_MAX_TEXTURE_COUNT]; // Used to see which mipmaps are loaded, 0 if all mips are loaded, ~0u if none are loaded
	//uint32_t materialTextureMipLevelReferenceCount[MATERIAL_MAX_TEXTURE_COUNT][16]; // A reference counter for each texture and each LOD to see which mips are being used, lower levels take priority
};

struct alignas(64) ModelMeshDrawPrimitive
{
	uint64_t materialID;

	uint32_t indexCount;
	uint32_t firstIndex;
};

struct ModelMeshNode
{
	glm::vec4 position_scale; // xyz - position, w - scale
	glm::quat orientation;

	std::vector<struct ModelMeshDrawPrimitive> drawPrimitives;
	std::vector<ModelMeshNode> children;
};

struct ModelResource
{
	uint64_t modelID;
	
	std::string sourceFile; // If empty() then there was no source file

	Buffer modelBuffer;

	std::vector<ModelMeshNode> meshNodes;
};

class KalosEngine;
class Renderer;

namespace tinygltf
{
	class TinyGLTF;
	struct Model;
	struct Image;
}

class ResourceManager
{
public:
	ResourceManager(KalosEngine *enginePtr);
	virtual ~ResourceManager();

	MaterialResource *loadMaterial(const MaterialDefinition &definition);

	bool importGLTFModel(const std::string &file);

	MaterialResource *getMaterial(uint64_t materialID);
	ModelResource *getModel(uint64_t modelID);

private:
	
	KalosEngine *engine;
	Renderer *renderer;

	CommandPool importResourceCmdPool;

	std::unique_ptr<tinygltf::TinyGLTF> gltfLoader;

	std::unordered_map<uint64_t, MaterialResource*> materialResources;
	std::unordered_map<uint64_t, ModelResource *> modelResources;

	bool importGLTFMaterials(MaterialResource *modelMaterialResources, tinygltf::Model &model, const std::string &file);
	std::vector<std::vector<uint8_t>> createImageMipmaps(const uint8_t *imageData, uint32_t component, uint32_t width, uint32_t height);

	static bool gltfImageLoadingFunction(tinygltf::Image *image, const int image_idx, std::string *err, std::string *warn, int req_width, int req_height, const unsigned char *bytes, int size, void *user_data);

	void saveKETTexture(const std::string &filename, ResourceFormat format, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, uint32_t arrayLayers, const void *textureData, size_t textureDataSize);
};

#endif /* RESOURCES_RESOURCEMANAGER_H_ */
