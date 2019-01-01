#include "RendererCore/Tests/TriangleTest.h"

#include <RendererCore/Renderer.h>

#include <GLFW/glfw3.h>

TriangleTest::TriangleTest(Renderer *rendererPtr)
{
	renderer = rendererPtr;

	renderTargetSampler = renderer->createSampler();

	{
		gfxGraph = renderer->createRenderGraph();

		RenderPassAttachment testOut;
		testOut.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
		testOut.namedRelativeSize = "swapchain";

		auto &test = gfxGraph->addRenderPass("test", RG_PIPELINE_GRAPHICS);
		test.addColorOutput("testOut", testOut, true, {0.75, 0.75, 0.1, 1});

		test.setInitFunction(std::bind(&TriangleTest::passInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&TriangleTest::passRender, this, std::placeholders::_1, std::placeholders::_2));

		gfxGraph->addNamedSize("swapchain", glm::uvec3(1920, 1080, 1));
		gfxGraph->setFrameGraphOutput("testOut");

		double sT = glfwGetTime();
		gfxGraph->build();
		Log::get()->info("RenderGraph build took {} ms", (glfwGetTime() - sT) * 1000.0);
	}

	renderTargetTV = gfxGraph->getRenderGraphOutputTextureView();
}

TriangleTest::~TriangleTest()
{
	renderer->destroySampler(renderTargetSampler);

	renderer->destroyPipeline(gfxPipeline);

	renderer->destroyRenderGraph(gfxGraph);
}

void TriangleTest::passInit(const RenderGraphInitFunctionData &data)
{
	createPipeline(data);
}

void TriangleTest::passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
	cmdBuffer->draw(3);
}

void TriangleTest::render()
{
	gfxGraph->execute();
}

void TriangleTest::createPipeline(const RenderGraphInitFunctionData &data)
{
	ShaderModule vertShader = renderer->createShaderModule("GameData/shaders/tests/triangle_test.hlsl", SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_HLSL, "TriangleTestVS");
	ShaderModule fragShader = renderer->createShaderModule("GameData/shaders/tests/triangle_test.hlsl", SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_HLSL, "TriangleTestFS");

	PipelineShaderStage vertShaderStage = {};
	vertShaderStage.entry = "TriangleTestVS"; // TODO Redundant, change to use the shader module entry point
	vertShaderStage.module = vertShader;

	PipelineShaderStage fragShaderStage = {};
	fragShaderStage.entry = "TriangleTestFS"; // TODO Redundant, change to use the shader module entry point
	fragShaderStage.module = fragShader;

	PipelineVertexInputInfo vertexInput = {};
	vertexInput.vertexInputAttribs = {};
	vertexInput.vertexInputBindings = {};

	PipelineInputAssemblyInfo inputAssembly = {};
	inputAssembly.primitiveRestart = false;
	inputAssembly.topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	PipelineViewportInfo viewportInfo = {};
	viewportInfo.scissors = {{0, 0, 1920, 1080}};
	viewportInfo.viewports = {{0, 0, 1920, 1080}};

	PipelineRasterizationInfo rastInfo = {};
	rastInfo.clockwiseFrontFace = false;
	rastInfo.cullMode = CULL_MODE_NONE;
	rastInfo.lineWidth = 1;
	rastInfo.polygonMode = POLYGON_MODE_FILL;
	rastInfo.rasterizerDiscardEnable = false;
	rastInfo.enableOutOfOrderRasterization = false;

	PipelineDepthStencilInfo depthInfo = {};
	depthInfo.enableDepthTest = false;
	depthInfo.enableDepthWrite = false;
	depthInfo.minDepthBounds = 0;
	depthInfo.maxDepthBounds = 1;
	depthInfo.depthCompareOp = COMPARE_OP_ALWAYS;

	PipelineColorBlendAttachment colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = false;
	colorBlendAttachment.colorWriteMask = COLOR_COMPONENT_R_BIT | COLOR_COMPONENT_G_BIT | COLOR_COMPONENT_B_BIT | COLOR_COMPONENT_A_BIT;

	PipelineColorBlendInfo colorBlend = {};
	colorBlend.attachments = {colorBlendAttachment};
	colorBlend.logicOpEnable = false;
	colorBlend.logicOp = LOGIC_OP_COPY;
	colorBlend.blendConstants[0] = 1.0f;
	colorBlend.blendConstants[1] = 1.0f;
	colorBlend.blendConstants[2] = 1.0f;
	colorBlend.blendConstants[3] = 1.0f;

	PipelineDynamicStateInfo dynamicState = {};
	dynamicState.dynamicStates = {};

	GraphicsPipelineInfo info = {};
	info.stages = {vertShaderStage, fragShaderStage};
	info.vertexInputInfo = vertexInput;
	info.inputAssemblyInfo = inputAssembly;
	info.viewportInfo = viewportInfo;
	info.rasterizationInfo = rastInfo;
	info.depthStencilInfo = depthInfo;
	info.colorBlendInfo = colorBlend;
	info.dynamicStateInfo = dynamicState;

	info.inputPushConstantRanges = {};
	info.inputSetLayouts = {{
		//{0, DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, SHADER_STAGE_FRAGMENT_BIT},
	}};

	gfxPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.module);
	renderer->destroyShaderModule(fragShaderStage.module);
}