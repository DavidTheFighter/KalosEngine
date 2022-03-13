#include "LightingRenderer.h"

#include <Game/KalosEngine.h>

#include <RendererCore/Renderer.h>

LightingRenderer::LightingRenderer(KalosEngine *enginePtr)
{
	engine = enginePtr;
	renderer = engine->renderer.get();
}

LightingRenderer::~LightingRenderer()
{
	renderer->destroyPipeline(lightingPipeline);
	renderer->destroyPipeline(tonemapPipeline);

	renderer->destroyDescriptorPool(lightingDescriptorPool);
	renderer->destroyDescriptorPool(tonemapDescriptorPool);
}

void LightingRenderer::lightingPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_COMPUTE, lightingPipeline);
	cmdBuffer->bindDescriptorSets(PIPELINE_BIND_POINT_COMPUTE, 0, {lightingDescriptorSet});
	cmdBuffer->dispatch(swapchainSize.x / 8, swapchainSize.y / 8, 1);
}

void LightingRenderer::tonemapPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	TonemapPushConstantsBuffer pushConstants = {};
	pushConstants.exposure = 1.0f;
	pushConstants.invGamma = 1.0f / 2.2f;

	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline);
	cmdBuffer->bindDescriptorSets(PIPELINE_BIND_POINT_COMPUTE, 0, {tonemapDescriptorSet});
	cmdBuffer->pushConstants(0, sizeof(TonemapPushConstantsBuffer), &pushConstants);
	cmdBuffer->dispatch(swapchainSize.x / 8, swapchainSize.y / 8, 1);
}

void LightingRenderer::lightingPassInit(const RenderGraphInitFunctionData &data)
{
	ShaderModule compShader = renderer->createShaderModule("GameData/shaders/lighting.hlsl", SHADER_STAGE_COMPUTE_BIT, SHADER_LANGUAGE_HLSL, "LightingCS");

	PipelineShaderStage compShaderStage = {};
	compShaderStage.shaderModule = compShader;

	DescriptorSetBinding gbuffer0 = {};
	gbuffer0.binding = 0;
	gbuffer0.arrayCount = 1;
	gbuffer0.type = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
	gbuffer0.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetBinding gbufferDepth = {};
	gbufferDepth.binding = 1;
	gbufferDepth.arrayCount = 1;
	gbufferDepth.type = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
	gbufferDepth.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetBinding lightingOutput = {};
	lightingOutput.binding = 2;
	lightingOutput.arrayCount = 1;
	lightingOutput.type = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	lightingOutput.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetLayoutDescription set0 = {};
	set0.bindings = {gbuffer0, gbufferDepth, lightingOutput};

	ComputePipelineInfo info = {};
	info.shader = compShaderStage;
	info.inputPushConstants = {0, 0};
	info.inputSetLayouts = {set0};

	lightingPipeline = renderer->createComputePipeline(info);

	renderer->destroyShaderModule(compShaderStage.shaderModule);

	lightingDescriptorPool = renderer->createDescriptorPool(set0, 1);
	lightingDescriptorSet = lightingDescriptorPool->allocateDescriptorSet();
}

void LightingRenderer::lightingPassDescriptorUpdate(const RenderGraphDescriptorUpdateFunctionData &data)
{
	DescriptorTextureInfo gbuffer0 = {};
	gbuffer0.layout = TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	gbuffer0.view = data.graphTextureViews.at("gbuffer0");

	DescriptorTextureInfo gbufferDepth = {};
	gbufferDepth.layout = TEXTURE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
	gbufferDepth.view = data.graphTextureViews.at("gbufferDepth");

	DescriptorTextureInfo lightingOutput = {};
	lightingOutput.layout = TEXTURE_LAYOUT_GENERAL;
	lightingOutput.view = data.graphTextureViews.at("lightingOutput");

	std::vector<DescriptorWriteInfo> writes(3);
	writes[0].descriptorType = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].textureInfo = {lightingOutput};

	writes[1].descriptorType = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].textureInfo = {gbufferDepth};
	
	writes[2].descriptorType = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	writes[2].dstBinding = 2;
	writes[2].dstArrayElement = 0;
	writes[2].textureInfo = {lightingOutput};

	renderer->writeDescriptorSets(lightingDescriptorSet, writes);
}

void LightingRenderer::tonemapPassInit(const RenderGraphInitFunctionData &data)
{
	ShaderModule compShader = renderer->createShaderModule("GameData/shaders/tonemap.hlsl", SHADER_STAGE_COMPUTE_BIT, SHADER_LANGUAGE_HLSL, "TonemapCS");

	PipelineShaderStage compShaderStage = {};
	compShaderStage.shaderModule = compShader;

	DescriptorSetBinding tonemapInput = {};
	tonemapInput.binding = 0;
	tonemapInput.arrayCount = 1;
	tonemapInput.type = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	tonemapInput.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetBinding tonemapOutput = {};
	tonemapOutput.binding = 1;
	tonemapOutput.arrayCount = 1;
	tonemapOutput.type = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	tonemapOutput.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetLayoutDescription set0 = {};
	set0.bindings = {tonemapInput, tonemapOutput};

	ComputePipelineInfo info = {};
	info.shader = compShaderStage;
	info.inputPushConstants = {sizeof(TonemapPushConstantsBuffer), SHADER_STAGE_COMPUTE_BIT};
	info.inputSetLayouts = {set0};

	tonemapPipeline = renderer->createComputePipeline(info);

	renderer->destroyShaderModule(compShaderStage.shaderModule);

	tonemapDescriptorPool = renderer->createDescriptorPool(set0, 1);
	tonemapDescriptorSet = tonemapDescriptorPool->allocateDescriptorSet();
}

void LightingRenderer::tonemapPassDescriptorUpdate(const RenderGraphDescriptorUpdateFunctionData &data)
{
	swapchainSize = glm::uvec2(data.graphTextureViews.at("tonemapOutput")->parentTexture->width, data.graphTextureViews.at("tonemapOutput")->parentTexture->height);

	DescriptorTextureInfo tonemapInputInfo = {};
	tonemapInputInfo.layout = TEXTURE_LAYOUT_GENERAL;
	tonemapInputInfo.view = data.graphTextureViews.at("lightingOutput");

	DescriptorTextureInfo tonemapOutputInfo = {};
	tonemapOutputInfo.layout = TEXTURE_LAYOUT_GENERAL;
	tonemapOutputInfo.view = data.graphTextureViews.at("tonemapOutput");

	std::vector<DescriptorWriteInfo> writes(2);
	writes[0].descriptorType = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].textureInfo = {tonemapInputInfo};

	writes[1].descriptorType = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].textureInfo = {tonemapOutputInfo};

	renderer->writeDescriptorSets(tonemapDescriptorSet, writes);
}
