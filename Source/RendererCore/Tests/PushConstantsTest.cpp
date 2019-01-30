#include "RendererCore/Tests/PushConstantsTest.h"

#include <RendererCore/Renderer.h>

/*

This tests the functionality of push constants. It should show two triangles rotated 90 degrees from each other, rotating with time.
Depth buffering isn't enabled for this so don't expect proper depth testing. This tests:
- Everything the triangle test does
- Push constants in pipeline creation and drawing
- HLSL compatilbity with push constants
- Setting the whole range of push constants with a single cmd
- Setting particular sections of the push constant range with multiple cmds

*/

PushConstantsTest::PushConstantsTest(Renderer *rendererPtr)
{
	renderer = rendererPtr;

	gfxPipeline = nullptr;

	renderTargetSampler = renderer->createSampler();

	{
		gfxGraph = renderer->createRenderGraph();

		RenderPassAttachment testOut;
		testOut.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
		testOut.namedRelativeSize = "swapchain";

		auto &test = gfxGraph->addRenderPass("test", RG_PIPELINE_GRAPHICS);
		test.addColorOutput("testOut", testOut, true, {0, 0, 0, 1});

		test.setInitFunction(std::bind(&PushConstantsTest::passInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&PushConstantsTest::passRender, this, std::placeholders::_1, std::placeholders::_2));

		gfxGraph->addNamedSize("swapchain", glm::uvec3(1920, 1080, 1));
		gfxGraph->setFrameGraphOutput("testOut");

		gfxGraph->build();
	}

	rotateCounter = 0.0f;
}

PushConstantsTest::~PushConstantsTest()
{
	renderer->destroySampler(renderTargetSampler);

	renderer->destroyPipeline(gfxPipeline);

	renderer->destroyRenderGraph(gfxGraph);
}

void PushConstantsTest::passInit(const RenderGraphInitFunctionData &data)
{
	if (gfxPipeline != nullptr)
		renderer->destroyPipeline(gfxPipeline);

	createPipeline(data);
}

void PushConstantsTest::passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	glm::mat4 camModlMat = glm::rotate(glm::mat4(1), rotateCounter, glm::vec3(0, 1, 0));
	glm::mat4 camViewMat = glm::lookAt(glm::vec3(7.5f, 3.0f, 0.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	glm::mat4 camProjMat = glm::perspective<float>(60.0f * (M_PI / 180.0f), 16 / 9.0f, 0.1f, 100.0f);
	glm::mat4 camMVPMat = camProjMat * camViewMat * camModlMat;

	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
	cmdBuffer->pushConstants(0, sizeof(glm::mat4), &camMVPMat);
	cmdBuffer->draw(3);

	camModlMat = glm::rotate(glm::mat4(1), rotateCounter + (float) (M_PI * 0.5f), glm::vec3(0, 1, 0));
	camMVPMat = camProjMat * camViewMat * camModlMat;

	cmdBuffer->pushConstants(sizeof(glm::vec4) * 0.0f, sizeof(glm::vec4), &camMVPMat[0][0]);
	cmdBuffer->pushConstants(sizeof(glm::vec4) * 1.0f, sizeof(glm::vec4), &camMVPMat[1][0]);
	cmdBuffer->pushConstants(sizeof(glm::vec4) * 2.0f, sizeof(glm::vec4), &camMVPMat[2][0]);
	cmdBuffer->pushConstants(sizeof(glm::vec4) * 3.0f, sizeof(glm::vec4), &camMVPMat[3][0]);

	cmdBuffer->draw(3);
}

void PushConstantsTest::render()
{
	renderDoneSemaphore = gfxGraph->execute();

	rotateCounter += 1 / 60.0f;
}

void PushConstantsTest::createPipeline(const RenderGraphInitFunctionData &data)
{
	ShaderModule vertShader = renderer->createShaderModule("GameData/shaders/tests/push_constants_test.hlsl", SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_HLSL, "PushConstantsTestVS");
	ShaderModule fragShader = renderer->createShaderModule("GameData/shaders/tests/push_constants_test.hlsl", SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_HLSL, "PushConstantsTestFS");

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

	info.inputPushConstants = {sizeof(glm::mat4), SHADER_STAGE_VERTEX_BIT};
	info.inputSetLayouts = {};

	gfxPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);
}