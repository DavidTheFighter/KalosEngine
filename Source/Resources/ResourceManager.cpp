#include "Resources/ResourceManager.h"

#include <Game/KalosEngine.h>

#include <RendererCore/Renderer.h>

#include <Resources/FileLoader.h>
#include <Resources/ResourceImporter.h>

#include <lodepng.h>
#include <picosha2.h>
#include <Compressonator.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>

#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE

#include <tiny_gltf.h>

ResourceManager::ResourceManager(KalosEngine *enginePtr)
{
	engine = enginePtr;
	renderer = engine->renderer.get();

	gltfLoader = std::unique_ptr<tinygltf::TinyGLTF>(new tinygltf::TinyGLTF());

	gltfLoader->SetImageLoader(&ResourceManager::gltfImageLoadingFunction, nullptr);

	importResourceCmdPool = renderer->createCommandPool(QUEUE_TYPE_TRANSFER, COMMAND_POOL_TRANSIENT_BIT);
}

ResourceManager::~ResourceManager()
{
	for (auto &materialIt : materialResources)
	{
		for (int i = 0; i < MATERIAL_MAX_TEXTURE_COUNT; i++)
		{
			if (materialIt.second->materialTextures[i] != nullptr)
			{
				renderer->destroyTexture(materialIt.second->materialTextures[i]);
				renderer->destroyTextureView(materialIt.second->materialTextureViews[i]);

				materialIt.second->materialTextures[i] = nullptr;
				materialIt.second->materialTextureViews[i] = nullptr;
			}
		}
	}

	for (auto &modelIt : modelResources)
	{
		if (modelIt.second->modelBuffer != nullptr)
		{
			renderer->destroyBuffer(modelIt.second->modelBuffer);
			modelIt.second->modelBuffer = nullptr;
		}
	}

	renderer->destroyCommandPool(importResourceCmdPool);
}

glm::vec2 encodeNormalSpheremap(glm::vec3 normal)
{
	normal /= (abs(normal.x) + abs(normal.y) + abs(normal.z));

	normal.x = normal.z >= 0.0f ? normal.x : ((1.0f - glm::abs(normal.y)) * (normal.x >= 0.0f ? 1.0f : -1.0f));
	normal.y = normal.z >= 0.0f ? normal.y : ((1.0f - glm::abs(normal.x)) * (normal.y >= 0.0f ? 1.0f : -1.0f));

	return glm::vec2(normal.x, normal.y);
}

MaterialResource *ResourceManager::loadMaterial(const MaterialDefinition &definition)
{
	MaterialResource *material = new MaterialResource();
	material->materialID = 0;
	material->pipelineID = definition.pipelineID;

	std::string hashString = "";

	for (int i = 0; i < MATERIAL_MAX_TEXTURE_COUNT; i++)
	{
		material->textureFiles[i] = definition.textureFiles[i];
		hashString += definition.textureFiles[i];
	}

	std::vector<uint8_t> hashStringHash(picosha2::k_digest_size);
	picosha2::hash256(hashString.begin(), hashString.end(), hashStringHash.begin(), hashStringHash.end());

	for (uint64_t i = 0; i < 8; i++)
		material->materialID |= uint64_t(hashStringHash[31 - i]) << (i * 8);

	StagingTexture materialStagingTextures[MATERIAL_MAX_TEXTURE_COUNT];

	for (int i = 0; i < MATERIAL_MAX_TEXTURE_COUNT; i++)
	{
		material->materialTextures[i] = nullptr;
		material->materialTextureViews[i] = nullptr;
		materialStagingTextures[i] = nullptr;

		if (material->textureFiles[i].empty())
			continue;

		std::vector<char> textureFileData = FileLoader::instance()->readFileBuffer(material->textureFiles[i]);
		std::vector<uint8_t> textureData;
		uint32_t width, height;

		lodepng::decode(textureData, width, height, reinterpret_cast<uint8_t *>(textureFileData.data()), textureFileData.size(), LCT_RGBA);

		materialStagingTextures[i] = renderer->createStagingTexture({width, height, 1}, RESOURCE_FORMAT_R8G8B8A8_UNORM, 1, 1);
		renderer->fillStagingTextureSubresource(materialStagingTextures[i], textureData.data(), 0, 0);

		material->materialTextures[i] = renderer->createTexture({width, height, 1}, RESOURCE_FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_TRANSFER_DST_BIT | TEXTURE_USAGE_SAMPLED_BIT, MEMORY_USAGE_GPU_ONLY, false, 1, 1, 1);
		material->materialTextureViews[i] = renderer->createTextureView(material->materialTextures[i]);
	}

	CommandPool tempCmdPool = renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT);
	Fence tempFence = renderer->createFence();
	CommandBuffer tempCmdBuffer = tempCmdPool->allocateCommandBuffer();
	tempCmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	for (int i = 0; i < MATERIAL_MAX_TEXTURE_COUNT; i++)
	{
		if (materialStagingTextures[i] != nullptr)
		{
			ResourceBarrier barrier0 = {};
			barrier0.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
			barrier0.textureTransition.oldLayout = TEXTURE_LAYOUT_INITIAL_STATE;
			barrier0.textureTransition.newLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier0.textureTransition.subresourceRange = {0, 1, 0, 1};
			barrier0.textureTransition.texture = material->materialTextures[i];

			ResourceBarrier barrier1 = {};
			barrier1.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
			barrier1.textureTransition.oldLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier1.textureTransition.newLayout = TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier1.textureTransition.subresourceRange = {0, 1, 0, 1};
			barrier1.textureTransition.texture = material->materialTextures[i];

			tempCmdBuffer->resourceBarriers({barrier0});
			tempCmdBuffer->stageTextureSubresources(materialStagingTextures[i], material->materialTextures[i]);
			tempCmdBuffer->resourceBarriers({barrier1});
		}
	}

	tempCmdBuffer->endCommands();
	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {tempCmdBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 5);

	renderer->destroyFence(tempFence);
	renderer->destroyCommandPool(tempCmdPool);

	for (int i = 0; i < MATERIAL_MAX_TEXTURE_COUNT; i++)
		if (materialStagingTextures[i] != nullptr)
			renderer->destroyStagingTexture(materialStagingTextures[i]);

	materialResources[material->materialID] = material;

	return material;
}

bool ResourceManager::importGLTFModel(const std::string &file)
{
	ModelResource *modelResource = new ModelResource();

	modelResource->sourceFile = file;

	std::vector<uint8_t> hashStringHash(picosha2::k_digest_size);
	picosha2::hash256(file.begin(), file.end(), hashStringHash.begin(), hashStringHash.end());

	for (uint64_t i = 0; i < 8; i++)
		modelResource->modelID |= uint64_t(hashStringHash[31 - i]) << (i * 8);

	tinygltf::Model model;
	std::string err, warn;

	std::vector<char> modelFileData = FileLoader::instance()->readFileBuffer(file);
	std::string modelFileBaseDirectory = FileLoader::instance()->getWorkingDir() + file.substr(0, file.find_last_of('/'));

	bool ret = false;

	if (file.substr(file.size() - 3, 3) == "glb")
		ret = gltfLoader->LoadBinaryFromMemory(&model, &err, &warn, reinterpret_cast<uint8_t *>(modelFileData.data()), modelFileData.size(), modelFileBaseDirectory);
	else if (file.substr(file.size() - 4, 4) == "gltf")
		ret = gltfLoader->LoadASCIIFromString(&model, &err, &warn, modelFileData.data(), modelFileData.size(), modelFileBaseDirectory, 1);
	else
	{
		Log::get()->error("ResourceManager: Cannot load file \"{}\" because it is not a .glb or .gltf file!");
		return false;
	}

	if (!warn.empty())
		Log::get()->warn("ResourceManager: Warning while loading glTF file \"{}\": \"{}\"", file, warn);

	if (!err.empty())
		Log::get()->warn("ResourceManager: Error while loading glTF file \"{}\": \"{}\"", file, err);

	if (!ret)
	{
		Log::get()->error("ResourceManager: Failed to load glTF file \"{}\"", file);
		return false;
	}

	MaterialResource *modelMaterialResources = new MaterialResource[model.materials.size()];

	if (!importGLTFMaterials(modelMaterialResources, model, file))
		return false;

	bool use32bitIndices = false;

	for (int i = 0; i < model.nodes.size(); i++)
	{
		const tinygltf::Node &gltfNode = model.nodes[i];
		
		if (gltfNode.mesh >= 0)
		{
			const tinygltf::Mesh &nodeMesh = model.meshes[gltfNode.mesh];

			for (int p = 0; p < nodeMesh.primitives.size(); p++)
			{
				const tinygltf::Primitive &primitive = nodeMesh.primitives[p];

				if (model.accessors[primitive.indices].componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
				{
					use32bitIndices = true;
					break;
				}
			}
		}

		if (use32bitIndices)
			break;
	}

	std::vector<uint8_t> modelIndexBuffer;
	std::vector<uint8_t> modelVertexBuffer;

	ModelMeshNode *meshNodes = new ModelMeshNode[model.nodes.size()];
	bool *meshNodesHasNoParent = new bool[model.nodes.size()];

	for (int n = 0; n < model.nodes.size(); n++)
	{
		const tinygltf::Node &gltfNode = model.nodes[n];

		ModelMeshNode &node = meshNodes[n];
		node = ModelMeshNode();

		meshNodesHasNoParent[n] = true;

		if (gltfNode.matrix.size() > 0)
		{
			glm::mat4 gltfNodeMatrix;

			for (int x = 0; x < 4; x++)
				for (int y = 0; y < 4; y++)
					gltfNodeMatrix[x][y] = gltfNode.matrix[y * 4 + x];

			glm::vec3 scale, translation, skew;
			glm::quat orientation;
			glm::vec4 perspective;

			glm::decompose(gltfNodeMatrix, scale, orientation, translation, skew, perspective);

			node.orientation = orientation;
			node.position_scale = glm::vec4(translation.x, translation.y, translation.z, (scale.x + scale.y + scale.z) / 3.0f);
		}
		else
		{
			if (gltfNode.rotation.size() > 0)
				node.orientation = glm::quat(gltfNode.rotation[0], gltfNode.rotation[1], gltfNode.rotation[2], gltfNode.rotation[3]);

			if (gltfNode.translation.size() > 0)
				node.position_scale = glm::vec4(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2], 1);

			if (gltfNode.scale.size() > 0)
				node.position_scale.w = (gltfNode.scale[0] + gltfNode.scale[1] + gltfNode.scale[2]) / 3.0f;
		}

		if (gltfNode.mesh >= 0)
		{
			const tinygltf::Mesh &mesh = model.meshes[gltfNode.mesh];

			for (int p = 0; p < mesh.primitives.size(); p++)
			{
				const tinygltf::Primitive &primitive = mesh.primitives[p];
				const tinygltf::Accessor &primitiveIndexAccessor = model.accessors[primitive.indices];

				ModelMeshDrawPrimitive drawPrimitive = {};
				drawPrimitive.materialID = modelMaterialResources[primitive.material].materialID;
				drawPrimitive.indexCount = primitiveIndexAccessor.count;
				drawPrimitive.firstIndex = primitiveIndexAccessor.byteOffset / (use32bitIndices ? 4 : 2);

				node.drawPrimitives.push_back(drawPrimitive);

				// Append the index data to our global model index buffer
				const tinygltf::BufferView &indexBufferView = model.bufferViews[primitiveIndexAccessor.bufferView];
				uint8_t *meshIndexBuffer = model.buffers[indexBufferView.buffer].data.data() + indexBufferView.byteOffset + primitiveIndexAccessor.byteOffset;

				uint32_t indexSize = primitiveIndexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ? 2 : 4;

				if ((!use32bitIndices && indexSize == 2) || (use32bitIndices && indexSize == 4))
				{
					modelIndexBuffer.insert(modelIndexBuffer.end(), meshIndexBuffer, meshIndexBuffer + primitiveIndexAccessor.count * indexSize);
				}
				else if (use32bitIndices && indexSize == 2)
				{
					for (size_t index = 0; index < primitiveIndexAccessor.count; index++)
					{
						uint32_t indiceValue = meshIndexBuffer[index * indexSize] + (meshIndexBuffer[index * indexSize + 1] << 8);
						modelIndexBuffer.insert(modelIndexBuffer.end(), reinterpret_cast<uint8_t *>(&indiceValue), reinterpret_cast<uint8_t *>(&indiceValue) + 4);
					}
				}

				if (primitive.attributes.count("POSITION") == 0)
				{
					Log::get()->error("ResourceManager: Cannot load a mesh primitive that has no vertex position data! File: \"{}\"", file);
					return false;
				}

				if (primitive.attributes.count("NORMAL") == 0)
				{
					Log::get()->error("ResourceManager: Cannot load a mesh primitive that has no vertex normal data! File: \"{}\"", file);
					return false;
				}

				if (primitive.attributes.count("TANGENT") == 0)
				{
					Log::get()->error("ResourceManager: Cannot load a mesh primitive that has no vertex tangent data! File: \"{}\"", file);
					return false;
				}

				if (primitive.attributes.count("TEXCOORD_0") == 0)
				{
					Log::get()->error("ResourceManager: Cannot load a mesh primitive that has no vertex texcoord/uv data! File: \"{}\"", file);
					return false;
				}


				// Append the vertex data to our global model vertex buffer
				if (primitive.attributes.count("POSITION") > 0)
				{
					const tinygltf::Accessor &vertexAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
					const tinygltf::Accessor &normalAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
					const tinygltf::Accessor &tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
					const tinygltf::Accessor &texcoord0Accessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];

					tinygltf::Accessor *texcoord1Accessor = primitive.attributes.count("TEXCOORD_1") > 0 ? &model.accessors[primitive.attributes.find("TEXCOORD_1")->second] : nullptr;

					if (vertexAccessor.count != normalAccessor.count || normalAccessor.count != tangentAccessor.count || tangentAccessor.count != texcoord0Accessor.count || (texcoord1Accessor == nullptr ? false : texcoord0Accessor.count != texcoord1Accessor->count))
					{
						Log::get()->error("ResourceManager: Cannot load mesh primitive that has inconsistent attribute counts! File: \"{}\"", file);
						return false;
					}

					uint8_t *vertexDataPtr = model.buffers[model.bufferViews[vertexAccessor.bufferView].buffer].data.data() + model.bufferViews[vertexAccessor.bufferView].byteOffset + vertexAccessor.byteOffset;
					uint8_t *normalDataPtr = model.buffers[model.bufferViews[normalAccessor.bufferView].buffer].data.data() + model.bufferViews[normalAccessor.bufferView].byteOffset + normalAccessor.byteOffset;
					uint8_t *tangentDataPtr = model.buffers[model.bufferViews[tangentAccessor.bufferView].buffer].data.data() + model.bufferViews[tangentAccessor.bufferView].byteOffset + tangentAccessor.byteOffset;
					uint8_t *texcoord0DataPtr = model.buffers[model.bufferViews[texcoord0Accessor.bufferView].buffer].data.data() + model.bufferViews[texcoord0Accessor.bufferView].byteOffset + texcoord0Accessor.byteOffset;
					uint8_t *texcoord1DataPtr = texcoord1Accessor == nullptr ? nullptr : model.buffers[model.bufferViews[texcoord1Accessor->bufferView].buffer].data.data() + model.bufferViews[texcoord1Accessor->bufferView].byteOffset + texcoord1Accessor->byteOffset;

					size_t prevSize = modelVertexBuffer.size();
					modelVertexBuffer.resize(modelVertexBuffer.size() + vertexAccessor.count * sizeof(NonSkinnedVertex));
					NonSkinnedVertex *vertexBufferDataPtr = reinterpret_cast<NonSkinnedVertex*>(&modelVertexBuffer[prevSize]);

					for (size_t i = 0; i < vertexAccessor.count; i++)
					{
						glm::vec4 tangent;
						glm::vec3 vertex, normal;
						glm::vec2 uv0, uv1;

						memcpy(&tangent, &tangentDataPtr[i * 4], sizeof(tangent));
						memcpy(&vertex, &vertexDataPtr[i * 3], sizeof(vertex));
						memcpy(&normal, &normalDataPtr[i * 3], sizeof(normal));
						memcpy(&uv0, &texcoord0DataPtr[i * 2], sizeof(uv0));
						
						if (texcoord1DataPtr != nullptr)
							memcpy(&uv1, &texcoord1DataPtr[i * 2], sizeof(uv1));

						vertexBufferDataPtr[i].vertex = vertex;
						vertexBufferDataPtr[i].tangentHandedness = tangent.w;
						vertexBufferDataPtr[i].normal = glm::vec4(normal.x, normal.y, normal.z, 0.0f);
						vertexBufferDataPtr[i].tangent = tangent;
						vertexBufferDataPtr[i].uv0 = uv0;
						vertexBufferDataPtr[i].uv1 = uv1;
					}
				}
				else
				{
					Log::get()->error("ResourceManager: Cannot load a mesh primitive that has no vertex position data! File: \"{}\"", file);
					return false;
				}
			}
		}
	}

	for (int n = 0; n < model.nodes.size(); n++)
	{
		const tinygltf::Node &gltfNode = model.nodes[n];

		for (int c = 0; c < gltfNode.children.size(); c++)
		{
			meshNodes[n].children.push_back(meshNodes[gltfNode.children[c]]);
			meshNodesHasNoParent[gltfNode.children[c]] = false;
		}
	}

	for (int n = 0; n < model.nodes.size(); n++)
		if (meshNodesHasNoParent[n] && (meshNodes[n].children.size() > 0 ? true : meshNodes[n].drawPrimitives.size() > 0)) // If this mesh node has no parent (aka it's a top node) and it's actually rendering something (aka it has draw primitives or it's children have draw primitives) then add it
			modelResource->meshNodes.push_back(meshNodes[n]);

	printf("%u - %u\n", modelIndexBuffer.size(), modelVertexBuffer.size());

	delete[] meshNodes;
	delete[] meshNodesHasNoParent;

	//importResourceCmdPool

	modelResource->modelBuffer = renderer->createBuffer(modelIndexBuffer.size() + modelVertexBuffer.size(), BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_VERTEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT, BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL, MEMORY_USAGE_GPU_ONLY, false);

	StagingBuffer modelStagingBuffer = renderer->createStagingBuffer(modelIndexBuffer.size() + modelVertexBuffer.size());
	
	uint8_t *modelStagingBufferData = reinterpret_cast<uint8_t *>(renderer->mapStagingBuffer(modelStagingBuffer));
	memcpy(modelStagingBufferData, modelIndexBuffer.data(), modelIndexBuffer.size() * sizeof(modelIndexBuffer[0]));
	memcpy(modelStagingBufferData + modelIndexBuffer.size() * sizeof(modelIndexBuffer[0]), modelVertexBuffer.data(), modelVertexBuffer.size() * sizeof(modelVertexBuffer[0]));
	renderer->unmapStagingBuffer(modelStagingBuffer);

	Fence tempFence = renderer->createFence();
	CommandBuffer tempCmdBuffer = importResourceCmdPool->allocateCommandBuffer();
	tempCmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	ResourceBarrier barrier0 = {};
	barrier0.barrierType = RESOURCE_BARRIER_TYPE_BUFFER_TRANSITION;
	barrier0.bufferTransition.oldLayout = BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier0.bufferTransition.newLayout = BUFFER_LAYOUT_VERTEX_INDEX_BUFFER;
	barrier0.bufferTransition.buffer = modelResource->modelBuffer;

	tempCmdBuffer->endCommands();
	renderer->submitToQueue(QUEUE_TYPE_TRANSFER, {tempCmdBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 5);
	renderer->destroyFence(tempFence);
	renderer->destroyStagingBuffer(modelStagingBuffer);
	importResourceCmdPool->resetCommandPoolAndFreeCommandBuffer(tempCmdBuffer);

	modelResources[modelResource->modelID] = modelResource;

	return true;
}

MaterialResource *ResourceManager::getMaterial(uint64_t materialID)
{
	auto materialIt = materialResources.find(materialID);

	return materialIt != materialResources.end() ? materialIt->second : nullptr;
}

ModelResource *ResourceManager::getModel(uint64_t modelID)
{
	auto modelIt = modelResources.find(modelID);

	return modelIt != modelResources.end() ? modelIt->second : nullptr;
}

bool ResourceManager::importGLTFMaterials(MaterialResource *modelMaterialResources, tinygltf::Model &model, const std::string &file)
{
	std::vector<uint8_t> blank16x16TextureData(16 * 16 * 4, 1);

	for (int m = 0; m < model.materials.size(); m++)
	{
		const tinygltf::Material &material = model.materials[m];

		printf("Material #%i - %s\n", m, material.name.c_str());

		uint8_t *albedoTextureData = nullptr, *normalsTextureData = nullptr, *roughnessMetalnessTextureData = nullptr, *AOTextureData = nullptr;
		uint8_t albedoTextureComponent = 4, normalsTextureComponent = 4, roughnessMetalnessTextureComponent = 4, AOTextureComponent = 4;
		glm::uvec2 albedoTextureSize = glm::uvec2(16, 16), normalsTextureSize = glm::uvec2(16, 16), roughnessMetalnessTextureSize = glm::uvec2(16, 16), AOTextureSize = glm::uvec2(16, 16);

		for (auto &param : material.values)
		{
			if (param.second.TextureTexCoord() != 0)
				Log::get()->warn("ResourceManager: Multiple texcoord/uv indices aren't supported yet! Your model \"{}\" probably won't render correctly\n", file);

			tinygltf::Image &textureImage = model.images[model.textures[param.second.TextureIndex()].source];

			if (textureImage.as_is)
			{
				Log::get()->error("ResourceManager: Cannot load material for model \"{}\", texture data is provided \"as is\" and is not supported!", file);
				return false;
			}

			if (textureImage.bits != 8)
			{
				Log::get()->error("ResourceManager: Image {} has a bitdepth of {}, must be 8! Model \"{}\"", param.first, textureImage.bits, file);
				return false;
			}

			if (param.first == "baseColorTexture")
			{
				albedoTextureSize = glm::uvec2(textureImage.width, textureImage.height);
				albedoTextureData = textureImage.image.data();
				albedoTextureComponent = textureImage.component;
			}
			else if (param.first == "metallicRoughnessTexture")
			{
				roughnessMetalnessTextureSize = glm::uvec2(textureImage.width, textureImage.height);
				roughnessMetalnessTextureData = textureImage.image.data();
				roughnessMetalnessTextureComponent = textureImage.component;
			}
		}

		for (auto &param : material.additionalValues)
		{
			if (param.second.TextureTexCoord() != 0)
				Log::get()->warn("ResourceManager: Multiple texcoord/uv indices aren't supported yet! Your model \"{}\" probably won't render correctly\n", file);

			tinygltf::Image &textureImage = model.images[model.textures[param.second.TextureIndex()].source];

			if (textureImage.as_is)
			{
				Log::get()->error("ResourceManager: Cannot load material for model \"{}\", texture data is provided \"as is\" and is not supported!", file);
				return false;
			}

			if (param.first == "normalTexture")
			{
				if (textureImage.bits != 8)
				{
					Log::get()->error("ResourceManager: Image {} has a bitdepth of {}, must be 8! Model \"{}\"", param.first, textureImage.bits, file);
					return false;
				}

				normalsTextureSize = glm::uvec2(textureImage.width, textureImage.height);
				normalsTextureData = textureImage.image.data();
				normalsTextureComponent = textureImage.component;
			}
			else if (param.first == "occlusionTexture")
			{
				if (textureImage.bits != 8)
				{
					Log::get()->error("ResourceManager: Image {} has a bitdepth of {}, must be 8! Model \"{}\"", param.first, textureImage.bits, file);
					return false;
				}

				AOTextureSize = glm::uvec2(textureImage.width, textureImage.height);
				AOTextureData = textureImage.image.data();
				AOTextureComponent = textureImage.component;
			}
		}

		modelMaterialResources[m] = MaterialResource();
		MaterialResource *materialResource = &modelMaterialResources[m];
		materialResource->materialID = 0;
		materialResource->pipelineID = 0;

		std::string hashString = file + "\\material#" + toString(m);

		std::vector<uint8_t> hashStringHash(picosha2::k_digest_size);
		picosha2::hash256(hashString.begin(), hashString.end(), hashStringHash.begin(), hashStringHash.end());

		for (uint64_t i = 0; i < 8; i++)
			materialResource->materialID |= uint64_t(hashStringHash[31 - i]) << (i * 8);

		auto compressTexture = [&blank16x16TextureData, this, file, m](uint32_t width, uint32_t height, uint32_t component, uint8_t *srcTextureData, CMP_FORMAT dstFormat, std::vector<uint8_t> &outTextureData)
		{
			CMP_CompressOptions options = {0};
			options.dwSize = sizeof(options);
			options.fquality = 0.05f;
			options.dwnumThreads = std::thread::hardware_concurrency();

			CMP_Texture srcTexture;
			srcTexture.dwSize = sizeof(CMP_Texture);
			srcTexture.dwWidth = width;
			srcTexture.dwHeight = height;
			srcTexture.dwPitch = width * sizeof(uint8_t) * component;
			srcTexture.dwDataSize = width * height * sizeof(uint8_t) * component;
			srcTexture.pData = srcTextureData == nullptr ? blank16x16TextureData.data() : srcTextureData;

			switch (component)
			{
				case 1:
					srcTexture.format = CMP_FORMAT_R_8;
					break;
				case 2:
					srcTexture.format = CMP_FORMAT_RG_8;
					break;
				case 3:
					srcTexture.format = CMP_FORMAT_RGB_888;
					break;
				case 4:
					srcTexture.format = CMP_FORMAT_RGBA_8888;
					break;
				default:
					srcTexture.format = CMP_FORMAT_Unknown;
			}

			CMP_Texture dstTexture;
			dstTexture.dwSize = sizeof(CMP_Texture);
			dstTexture.dwWidth = width;
			dstTexture.dwHeight = height;
			dstTexture.dwPitch = 0;
			dstTexture.format = dstFormat;
			dstTexture.nBlockWidth = 4;
			dstTexture.nBlockHeight = 4;
			dstTexture.nBlockDepth = 1;
			dstTexture.dwDataSize = CMP_CalculateBufferSize(&dstTexture);
			dstTexture.pData = new uint8_t[dstTexture.dwDataSize];

			double sT = engine->getTime();
			CMP_ERROR   cmp_status;
			cmp_status = CMP_ConvertTexture(&srcTexture, &dstTexture, &options, nullptr, NULL, NULL);
			if (cmp_status != CMP_OK)
			{
				Log::get()->error("ResourceManager: Couldn't compress texture! Error: {}, Model: {}, Material #{}\n", cmp_status, file, m);
				return;
			}

			printf("Compression took: %fms\n", (engine->getTime() - sT) * 1000.0);

			outTextureData.insert(outTextureData.end(), dstTexture.pData, dstTexture.pData + dstTexture.dwDataSize);

			delete[] dstTexture.pData;
		};

		std::vector<std::vector<uint8_t>> inAlbedoTextureData = createImageMipmaps(albedoTextureData, albedoTextureComponent, albedoTextureSize.x, albedoTextureSize.y);
		std::vector<std::vector<uint8_t>> inNormalTextureData = createImageMipmaps(normalsTextureData, normalsTextureComponent, normalsTextureSize.x, normalsTextureSize.y);

		std::vector<std::vector<uint8_t>> outAlbedoTextureData(inAlbedoTextureData.size()), outNormalsTextureData(inNormalTextureData.size());

		StagingTexture albedoStagingTexture = renderer->createStagingTexture({albedoTextureSize.x, albedoTextureSize.y, 1}, RESOURCE_FORMAT_BC7_UNORM_BLOCK, (uint32_t)outAlbedoTextureData.size(), 1);
		StagingTexture normalsStagingTexture = renderer->createStagingTexture({normalsTextureSize.x, normalsTextureSize.y, 1}, RESOURCE_FORMAT_BC7_UNORM_BLOCK, (uint32_t)outNormalsTextureData.size(), 1);

		for (size_t m = 0; m < inAlbedoTextureData.size(); m++)
		{
			compressTexture(std::max(albedoTextureSize.x >> m, 1u), std::max(albedoTextureSize.y >> m, 1u), albedoTextureComponent, inAlbedoTextureData[m].data(), CMP_FORMAT_BC7, outAlbedoTextureData[m]);
			renderer->fillStagingTextureSubresource(albedoStagingTexture, outAlbedoTextureData[m].data(), m, 0);
		}

		for (size_t m = 0; m < inNormalTextureData.size(); m++)
		{
			compressTexture(std::max(normalsTextureSize.x >> m, 1u), std::max(normalsTextureSize.y >> m, 1u), normalsTextureComponent, inNormalTextureData[m].data(), CMP_FORMAT_BC7, outNormalsTextureData[m]);
			renderer->fillStagingTextureSubresource(normalsStagingTexture, outNormalsTextureData[m].data(), m, 0);
		}

		materialResource->materialTextures[0] = renderer->createTexture({albedoTextureSize.x, albedoTextureSize.y, 1}, RESOURCE_FORMAT_BC7_UNORM_BLOCK, TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT, MEMORY_USAGE_GPU_ONLY, false, (uint32_t)outAlbedoTextureData.size(), 1, 1);
		materialResource->materialTextures[1] = renderer->createTexture({normalsTextureSize.x, normalsTextureSize.y, 1}, RESOURCE_FORMAT_BC7_UNORM_BLOCK, TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT, MEMORY_USAGE_GPU_ONLY, false, (uint32_t)outNormalsTextureData.size(), 1, 1);

		materialResource->materialTextureViews[0] = renderer->createTextureView(materialResource->materialTextures[0], TEXTURE_VIEW_TYPE_2D, {0, (uint32_t)outAlbedoTextureData.size(), 0, 1});
		materialResource->materialTextureViews[1] = renderer->createTextureView(materialResource->materialTextures[1], TEXTURE_VIEW_TYPE_2D, {0, (uint32_t)outNormalsTextureData.size(), 0, 1});

		CommandPool tempCmdPool = renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT);
		Fence tempFence = renderer->createFence();
		CommandBuffer tempCmdBuffer = tempCmdPool->allocateCommandBuffer();
		tempCmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		for (uint32_t m = 0; m < (uint32_t)inAlbedoTextureData.size(); m++)
		{
			ResourceBarrier barrier0 = {};
			barrier0.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
			barrier0.textureTransition.oldLayout = TEXTURE_LAYOUT_INITIAL_STATE;
			barrier0.textureTransition.newLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier0.textureTransition.subresourceRange = {m, 1, 0, 1};
			barrier0.textureTransition.texture = materialResource->materialTextures[0];

			ResourceBarrier barrier1 = {};
			barrier1.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
			barrier1.textureTransition.oldLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier1.textureTransition.newLayout = TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier1.textureTransition.subresourceRange = {m, 1, 0, 1};
			barrier1.textureTransition.texture = materialResource->materialTextures[0];

			tempCmdBuffer->resourceBarriers({barrier0});
			tempCmdBuffer->stageTextureSubresources(albedoStagingTexture, materialResource->materialTextures[0], {m, 1, 0, 1});
			tempCmdBuffer->resourceBarriers({barrier1});
		}

		for (uint32_t m = 0; m < (uint32_t)inNormalTextureData.size() - 2; m++)
		{
			ResourceBarrier barrier0 = {};
			barrier0.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
			barrier0.textureTransition.oldLayout = TEXTURE_LAYOUT_INITIAL_STATE;
			barrier0.textureTransition.newLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier0.textureTransition.subresourceRange = {m, 1, 0, 1};
			barrier0.textureTransition.texture = materialResource->materialTextures[1];

			ResourceBarrier barrier1 = {};
			barrier1.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
			barrier1.textureTransition.oldLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier1.textureTransition.newLayout = TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier1.textureTransition.subresourceRange = {m, 1, 0, 1};
			barrier1.textureTransition.texture = materialResource->materialTextures[1];

			tempCmdBuffer->resourceBarriers({barrier0});
			tempCmdBuffer->stageTextureSubresources(normalsStagingTexture, materialResource->materialTextures[1], {m, 1, 0, 1});
			tempCmdBuffer->resourceBarriers({barrier1});
		}

		tempCmdBuffer->endCommands();
		renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {tempCmdBuffer}, {}, {}, {}, tempFence);

		renderer->waitForFence(tempFence, 5);

		renderer->destroyFence(tempFence);
		renderer->destroyCommandPool(tempCmdPool);

		renderer->destroyStagingTexture(albedoStagingTexture);
		renderer->destroyStagingTexture(normalsStagingTexture);

		materialResources[materialResource->materialID] = materialResource;

		//saveKETTexture("GameData/textures/test.ket");
		//printf("%p %ux%u, %p %ux%u, %p %ux%u, %p %ux%u\n", albedoTextureData, albedoTextureSize.x, albedoTextureSize.y, normalsTextureData, normalsTextureSize.x, normalsTextureSize.y, roughnessMetalnessTextureData, roughnessMetalnessTextureSize.x, roughnessMetalnessTextureSize.y, AOTextureData, AOTextureSize.x, AOTextureSize.y);
	}

	return true;
}

std::vector<std::vector<uint8_t>> ResourceManager::createImageMipmaps(const uint8_t *imageData, uint32_t component, uint32_t width, uint32_t height)
{
	uint32_t mipLevelCount = uint32_t(std::log2(glm::max(width, height))) + 1;

	std::vector<std::vector<uint8_t>> imageMipmaps(mipLevelCount);

	for (uint32_t m = 0; m < mipLevelCount; m++)
		imageMipmaps[m].resize(std::max(width >> m, 4u) * std::max(height >> m, 1u) * component);

	// Mip 0
	memcpy(imageMipmaps[0].data(), imageData, width * height * component);

	for (uint32_t m = 1; m < mipLevelCount; m++)
	{
		uint32_t mipWidth = std::max(width >> m, 1u);
		uint32_t mipHeight = std::max(height >> m, 1u);

		uint32_t higherMipWidth = std::max(width >> (m - 1), 1u);
		uint32_t higherMipHeight = std::max(height >> (m - 1), 1u);

		for (uint32_t y = 0; y < mipHeight; y++)
		{
			for (uint32_t x = 0; x < mipWidth; x++)
			{
				for (uint32_t c = 0; c < component; c++)
				{
					imageMipmaps[m][(y * mipWidth + x) * component + c] = uint32_t(
						uint32_t(imageMipmaps[m - 1][(std::min(y * 2, higherMipHeight - 1) * higherMipWidth + std::min(x * 2, higherMipWidth - 1)) * component + c]) +
						uint32_t(imageMipmaps[m - 1][(std::min(y * 2, higherMipHeight - 1) * higherMipWidth + std::min(x * 2 + 1, higherMipWidth - 1)) * component + c]) +
						uint32_t(imageMipmaps[m - 1][(std::min(y * 2 + 1, higherMipHeight - 1) * higherMipWidth + std::min(x * 2, higherMipWidth - 1)) * component + c]) +
						uint32_t(imageMipmaps[m - 1][(std::min(y * 2 + 1, higherMipHeight - 1) * higherMipWidth + std::min(x * 2 + 1, higherMipWidth - 1)) * component + c])) / 4;
				}
			}
		}
	}

	return imageMipmaps;
}

void ResourceManager::saveKETTexture(const std::string &filename, ResourceFormat format, uint32_t width, uint32_t height, uint32_t depth, uint32_t mipLevels, uint32_t arrayLayers, const void *textureData, size_t textureDataSize)
{
	std::ofstream file(filename, std::ios::out | std::ios::binary);

	if (!file.is_open())
	{
		Log::get()->error("Failed to open file: {} for writing", filename);

		return;
	}
	
	KETHeader header = {};
	header.magic = 0x2054454b;
	header.format = uint32_t(format);
	header.width = width;
	header.height = height;
	header.depth = depth;
	header.mipLevels = mipLevels;
	header.arrayLayers = arrayLayers;
	
	file.write(reinterpret_cast<const char*>(&header), sizeof(KETHeader));
	file.write(reinterpret_cast<const char*>(textureData), textureDataSize);

	file.close();
}

bool ResourceManager::gltfImageLoadingFunction(tinygltf::Image *image, const int image_idx, std::string *err, std::string *warn, int req_width, int req_height, const unsigned char *bytes, int size, void *user_data)
{
	return tinygltf::LoadImageData(image, image_idx, err, warn, req_width, req_height, bytes, size, user_data);
}