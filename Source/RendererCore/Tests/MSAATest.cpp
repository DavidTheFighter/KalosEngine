#include "RendererCore/Tests/MSAATest.h"

#include <RendererCore/Renderer.h>

/*

This is the most basic test, just rendering a triangle with a specific clear color. It tests the proper operation of:
- Basic, one stage render graphs (validate, build, resource assignment, execution)
- Resizing the swapchain (look for jagged edges when upscaling the window size on the triangle)
- Basic pipeline usage (culling, wind order, viewport, scissor, no descriptors)
- Command buffers (only really bindPipeline and draw, but some backend stuff too)
- HLSL compatiblity between all backends
- Clear colors, resource formats
- Color attachment/render target
- Should theoretically test semaphores, but won't always show an error if they're being misused
*/

MSAATest::MSAATest(Renderer *rendererPtr)
{
	renderer = rendererPtr;

	gfxPipeline = nullptr;

	renderTargetSampler = renderer->createSampler(SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, SAMPLER_FILTER_NEAREST, SAMPLER_FILTER_NEAREST);

	{
		gfxGraph = renderer->createRenderGraph();

		RenderPassAttachment msaaRT;
		msaaRT.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
		msaaRT.namedRelativeSize = "swapchain";
		msaaRT.samples = 8;

		RenderPassAttachment resolvedOut;
		resolvedOut.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
		resolvedOut.namedRelativeSize = "swapchain";

		auto &test = gfxGraph->addRenderPass("msaaRender", RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS);
		test.addColorAttachmentOutput("msaaRT", msaaRT, true, {0, 0, 0, 1});

		test.setInitFunction(std::bind(&MSAATest::passInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&MSAATest::passRender, this, std::placeholders::_1, std::placeholders::_2));

		auto &msaaResolve = gfxGraph->addRenderPass("msaaRender", RENDER_GRAPH_PIPELINE_TYPE_GENERAL);
		msaaResolve.addStorageTexture("msaaRT", msaaRT, true, false, TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL, TEXTURE_LAYOUT_RESOLVE_SRC_OPTIMAL);
		msaaResolve.addStorageTexture("resolvedOut", resolvedOut, false, true, TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL, TEXTURE_LAYOUT_RESOLVE_DST_OPTIMAL);

		msaaResolve.setDescriptorUpdateFunction(std::bind(&MSAATest::resolvePassDescriptorUpdate, this, std::placeholders::_1));
		msaaResolve.setRenderFunction(std::bind(&MSAATest::resolvePassRender, this, std::placeholders::_1, std::placeholders::_2));

		gfxGraph->addNamedSize("swapchain", glm::uvec2(128, 72));
		gfxGraph->setRenderGraphOutput("resolvedOut");

		gfxGraph->build();
	}
}

MSAATest::~MSAATest()
{
	renderer->destroySampler(renderTargetSampler);

	renderer->destroyPipeline(gfxPipeline);

	renderer->destroyRenderGraph(gfxGraph);
}

void MSAATest::passInit(const RenderGraphInitFunctionData &data)
{
	if (gfxPipeline != nullptr)
		renderer->destroyPipeline(gfxPipeline);

	createPipeline(data);
}

void MSAATest::passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
	cmdBuffer->draw(3);
}

void MSAATest::resolvePassDescriptorUpdate(const RenderGraphDescriptorUpdateFunctionData &data)
{
	msaaTexture = data.graphTextureViews.at("msaaRT")->parentTexture;
	resolvedTexture = data.graphTextureViews.at("resolvedOut")->parentTexture;
}

void MSAATest::resolvePassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->resolveTexture(msaaTexture, resolvedTexture, {0, 1, 0, 1});
}

void MSAATest::render()
{
	renderDoneSemaphore = gfxGraph->execute(true);
}

void MSAATest::createPipeline(const RenderGraphInitFunctionData &data)
{
	ShaderModule vertShader = renderer->createShaderModule("GameData/shaders/tests/triangle_test.hlsl", SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_HLSL, "TriangleTestVS");
	ShaderModule fragShader = renderer->createShaderModule("GameData/shaders/tests/triangle_test.hlsl", SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_HLSL, "TriangleTestFS");

	PipelineShaderStage vertShaderStage = {};
	vertShaderStage.shaderModule = vertShader;

	PipelineShaderStage fragShaderStage = {};
	fragShaderStage.shaderModule = fragShader;

	PipelineVertexInputInfo vertexInput = {};
	vertexInput.vertexInputAttribs = {};
	vertexInput.vertexInputBindings = {};

	PipelineInputAssemblyInfo inputAssembly = {};
	inputAssembly.primitiveRestart = false;
	inputAssembly.topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	PipelineRasterizationInfo rastInfo = {};
	rastInfo.clockwiseFrontFace = false;
	rastInfo.cullMode = POLYGON_CULL_MODE_NONE;
	rastInfo.polygonMode = POLYGON_MODE_FILL;
	rastInfo.enableOutOfOrderRasterization = false;
	rastInfo.multiSampleCount = 8;

	PipelineDepthStencilInfo depthInfo = {};
	depthInfo.enableDepthTest = false;
	depthInfo.enableDepthWrite = false;
	depthInfo.depthCompareOp = COMPARE_OP_ALWAYS;

	PipelineColorBlendAttachment colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = false;
	colorBlendAttachment.colorWriteMask = COLOR_COMPONENT_R_BIT | COLOR_COMPONENT_G_BIT | COLOR_COMPONENT_B_BIT | COLOR_COMPONENT_A_BIT;

	PipelineColorBlendInfo colorBlend = {};
	colorBlend.attachments = {colorBlendAttachment};
	colorBlend.logicOpEnable = false;

	GraphicsPipelineInfo info = {};
	info.stages = {vertShaderStage, fragShaderStage};
	info.vertexInputInfo = vertexInput;
	info.inputAssemblyInfo = inputAssembly;
	info.rasterizationInfo = rastInfo;
	info.depthStencilInfo = depthInfo;
	info.colorBlendInfo = colorBlend;

	info.inputPushConstants = {0, 0};
	info.inputSetLayouts = {};

	gfxPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);
}