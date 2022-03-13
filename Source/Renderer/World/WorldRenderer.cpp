#include "WorldRenderer.h"

#include <Game/KalosEngine.h>

#include <RendererCore/Renderer.h>

#include <Resources/ResourceManager.h>

WorldRenderer::WorldRenderer(KalosEngine *enginePtr)
{
	engine = enginePtr;
	renderer = enginePtr->renderer.get();

	testMaterialPipeline = nullptr;

	MaterialDefinition pavingStones36 = {};
	pavingStones36.pipelineID = 0;
	pavingStones36.textureFiles[0] = "GameData/textures/PavingStones36/PavingStones36_col.png";
	//pavingStones36.textureFiles[1] = "GameData/textures/PavingStones36/PavingStones36_nrm.png";
	//pavingStones36.textureFiles[2] = "GameData/textures/PavingStones36/PavingStones36_rgh.png";

	engine->resourceManager->loadMaterial(pavingStones36);
	//engine->resourceManager->importGLTFModel("GameData/meshes/glTF/SciFiHelmet.gltf");
	//engine->resourceManager->importGLTFModel("GameData/meshes/test-bridge.glb");
}

WorldRenderer::~WorldRenderer()
{
	renderer->destroyPipeline(testMaterialPipeline);
}

void WorldRenderer::update(float delta)
{

}

void WorldRenderer::gbufferPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{

}

void WorldRenderer::gbufferPassInit(const RenderGraphInitFunctionData &data)
{
	ShaderModule vertShader = renderer->createShaderModule("GameData/shaders/test-material.hlsl", SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_HLSL, "TestMaterialVS");
	ShaderModule fragShader = renderer->createShaderModule("GameData/shaders/test-material.hlsl", SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_HLSL, "TestMaterialFS");

	PipelineShaderStage vertShaderStage = {};
	vertShaderStage.shaderModule = vertShader;

	PipelineShaderStage fragShaderStage = {};
	fragShaderStage.shaderModule = fragShader;

	VertexInputBinding vertexMeshBinding = {};
	vertexMeshBinding.binding = 0;
	vertexMeshBinding.stride = (3 + 2 + 3 + 3) * sizeof(float);
	vertexMeshBinding.inputRate = VERTEX_INPUT_RATE_VERTEX;

	VertexInputBinding instanceBinding = {};
	instanceBinding.binding = 1;
	instanceBinding.stride = (4 + 4) * sizeof(float);
	instanceBinding.inputRate = VERTEX_INPUT_RATE_INSTANCE;

	VertexInputAttribute meshVertexAttrib = {};
	meshVertexAttrib.binding = 0;
	meshVertexAttrib.location = 0;
	meshVertexAttrib.format = RESOURCE_FORMAT_R32G32B32A32_SFLOAT;
	meshVertexAttrib.offset = offsetof(NonSkinnedVertex, vertex);

	VertexInputAttribute meshNormalAttrib = {};
	meshNormalAttrib.binding = 0;
	meshNormalAttrib.location = 2;
	meshNormalAttrib.format = RESOURCE_FORMAT_R32G32B32_SFLOAT;
	meshNormalAttrib.offset = offsetof(NonSkinnedVertex, normal);

	VertexInputAttribute meshTangentAttrib = {};
	meshTangentAttrib.binding = 0;
	meshTangentAttrib.location = 3;
	meshTangentAttrib.format = RESOURCE_FORMAT_R32G32B32_SFLOAT;
	meshTangentAttrib.offset = offsetof(NonSkinnedVertex, tangent);

	VertexInputAttribute meshUV0Attrib = {};
	meshUV0Attrib.binding = 0;
	meshUV0Attrib.location = 1;
	meshUV0Attrib.format = RESOURCE_FORMAT_R32G32_SFLOAT;
	meshUV0Attrib.offset = offsetof(NonSkinnedVertex, uv0);

	VertexInputAttribute instancePositionScaleAttrib = {};
	instancePositionScaleAttrib.binding = 1;
	instancePositionScaleAttrib.location = 4;
	instancePositionScaleAttrib.format = RESOURCE_FORMAT_R32G32B32A32_SFLOAT;
	instancePositionScaleAttrib.offset = 0;

	VertexInputAttribute instanceQuatAttrib = {};
	instanceQuatAttrib.binding = 1;
	instanceQuatAttrib.location = 5;
	instanceQuatAttrib.format = RESOURCE_FORMAT_R32G32B32A32_SFLOAT;
	instanceQuatAttrib.offset = 4 * sizeof(float);

	PipelineVertexInputInfo vertexInput = {};
	vertexInput.vertexInputBindings = {vertexMeshBinding, instanceBinding};
	vertexInput.vertexInputAttribs = {meshVertexAttrib, meshUV0Attrib, meshNormalAttrib, meshTangentAttrib, instancePositionScaleAttrib, instanceQuatAttrib};

	PipelineInputAssemblyInfo inputAssembly = {};
	inputAssembly.primitiveRestart = false;
	inputAssembly.topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	PipelineRasterizationInfo rastInfo = {};
	rastInfo.clockwiseFrontFace = false;
	rastInfo.cullMode = POLYGON_CULL_MODE_BACK;
	rastInfo.polygonMode = POLYGON_MODE_FILL;
	rastInfo.enableOutOfOrderRasterization = true;

	PipelineDepthStencilInfo depthInfo = {};
	depthInfo.enableDepthTest = true;
	depthInfo.enableDepthWrite = true;
	depthInfo.depthCompareOp = COMPARE_OP_GREATER;

	PipelineColorBlendAttachment colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = false;
	colorBlendAttachment.colorWriteMask = COLOR_COMPONENT_R_BIT | COLOR_COMPONENT_G_BIT | COLOR_COMPONENT_B_BIT | COLOR_COMPONENT_A_BIT;

	PipelineColorBlendInfo colorBlend = {};
	colorBlend.attachments = {colorBlendAttachment};
	colorBlend.logicOpEnable = false;

	DescriptorStaticSampler staticSampler = {};
	staticSampler.magFilter = SAMPLER_FILTER_LINEAR;
	staticSampler.maxAnisotropy = 4.0f;

	DescriptorSetBinding materialSamplerBinding = {};
	materialSamplerBinding.binding = 0;
	materialSamplerBinding.arrayCount = 1;
	materialSamplerBinding.stageAccessMask = SHADER_STAGE_FRAGMENT_BIT;
	materialSamplerBinding.type = DESCRIPTOR_TYPE_SAMPLER;
	materialSamplerBinding.staticSamplers = {staticSampler};

	DescriptorSetBinding materialWorldDataBinding = {};
	materialWorldDataBinding.binding = 1;
	materialWorldDataBinding.arrayCount = 1;
	materialWorldDataBinding.stageAccessMask = SHADER_STAGE_VERTEX_BIT;
	materialWorldDataBinding.type = DESCRIPTOR_TYPE_CONSTANT_BUFFER;

	DescriptorSetBinding materialTexturesBinding = {};
	materialTexturesBinding.binding = 2;
	materialTexturesBinding.arrayCount = 64;
	materialTexturesBinding.stageAccessMask = SHADER_STAGE_FRAGMENT_BIT;
	materialTexturesBinding.type = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;

	DescriptorSetLayoutDescription set0 = {};
	set0.bindings = {materialSamplerBinding, materialWorldDataBinding, materialTexturesBinding};

	GraphicsPipelineInfo info = {};
	info.stages = {vertShaderStage, fragShaderStage};
	info.vertexInputInfo = vertexInput;
	info.inputAssemblyInfo = inputAssembly;
	info.rasterizationInfo = rastInfo;
	info.depthStencilInfo = depthInfo;
	info.colorBlendInfo = colorBlend;

	info.inputPushConstants = {sizeof(WorldRendererMaterialPushConstants), SHADER_STAGE_FRAGMENT_BIT};
	info.inputSetLayouts = {set0};

	testMaterialPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);
}