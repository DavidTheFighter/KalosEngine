#include "RendererCore/Tests/VertexIndexBufferTest.h"

#include <RendererCore/Renderer.h>

/*

Tests drawing using plain vertex buffers and a vertex buffer/index buffer combo. As a list:
- Everything the triangle test does
- Vertex buffer binding/locations
- Index buffer
- Staging buffers (for uploading vertex/index data)
- Basic command pool/command buffer operation (beginCommands, stageBuffer, endCommands, submitToQueue)
- Fences (to wait to destroy staging buffers after staging data is done)

The end result should be two triangles side by side on a black background. The left triangle uses vertex
buffers exlusively while the right one uses both vertex and index buffers.

*/

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

VertexIndexBufferTest::VertexIndexBufferTest(Renderer *rendererPtr)
{
	renderer = rendererPtr;

	gfxPipeline = nullptr;

	renderTargetSampler = renderer->createSampler();
	cmdPool = renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT);

	{
		gfxGraph = renderer->createRenderGraph();

		RenderPassAttachment testOut;
		testOut.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
		testOut.namedRelativeSize = "swapchain";

		auto &test = gfxGraph->addRenderPass("test", RG_PIPELINE_GRAPHICS);
		test.addColorOutput("testOut", testOut, true, {0, 0, 0, 1});

		test.setInitFunction(std::bind(&VertexIndexBufferTest::passInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&VertexIndexBufferTest::passRender, this, std::placeholders::_1, std::placeholders::_2));

		gfxGraph->addNamedSize("swapchain", glm::uvec3(1920, 1080, 1));
		gfxGraph->setFrameGraphOutput("testOut");

		gfxGraph->build();
	}

	createBuffers();
}

VertexIndexBufferTest::~VertexIndexBufferTest()
{
	renderer->destroySampler(renderTargetSampler);

	for (uint32_t i = 0; i < 2; i++)
		renderer->destroyBuffer(positionBuffers[i]);

	renderer->destroyBuffer(indexBuffer);
	renderer->destroyBuffer(colorBuffer);

	renderer->destroyCommandPool(cmdPool);

	renderer->destroyPipeline(gfxPipeline);

	renderer->destroyRenderGraph(gfxGraph);
}

void VertexIndexBufferTest::passInit(const RenderGraphInitFunctionData &data)
{
	if (gfxPipeline != nullptr)
		renderer->destroyPipeline(gfxPipeline);

	createPipeline(data);
}

void VertexIndexBufferTest::passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
	cmdBuffer->bindVertexBuffers(0, {positionBuffers[0], colorBuffer}, {0, 0});
	cmdBuffer->draw(3);

	cmdBuffer->bindIndexBuffer(indexBuffer, 0);
	cmdBuffer->bindVertexBuffers(0, {positionBuffers[1], colorBuffer}, {0, 0});
	cmdBuffer->draw(3);
}

void VertexIndexBufferTest::render()
{
	renderDoneSemaphore = gfxGraph->execute();
}

void VertexIndexBufferTest::createPipeline(const RenderGraphInitFunctionData &data)
{
	ShaderModule vertShader = renderer->createShaderModule("GameData/shaders/tests/vertex_index_buffer_test.hlsl", SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_HLSL, "VertexIndexBufferTestVS");
	ShaderModule fragShader = renderer->createShaderModule("GameData/shaders/tests/vertex_index_buffer_test.hlsl", SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_HLSL, "VertexIndexBufferTestFS");

	PipelineShaderStage vertShaderStage = {};
	vertShaderStage.shaderModule = vertShader;

	PipelineShaderStage fragShaderStage = {};
	fragShaderStage.shaderModule = fragShader;

	std::vector<VertexInputBinding> bindingDesc = std::vector<VertexInputBinding>(2);
	bindingDesc[0].binding = 0;
	bindingDesc[0].stride = sizeof(glm::vec4) * 2;
	bindingDesc[0].inputRate = VERTEX_INPUT_RATE_VERTEX;

	bindingDesc[1].binding = 1;
	bindingDesc[1].stride = sizeof(glm::vec4);
	bindingDesc[1].inputRate = VERTEX_INPUT_RATE_VERTEX;

	std::vector<VertexInputAttribute> attribDesc = std::vector<VertexInputAttribute>(3);
	attribDesc[0].binding = 0;
	attribDesc[0].location = 0;
	attribDesc[0].format = RESOURCE_FORMAT_R32G32B32_SFLOAT;
	attribDesc[0].offset = 0;

	attribDesc[1].binding = 0;
	attribDesc[1].location = 1;
	attribDesc[1].format = RESOURCE_FORMAT_R32G32B32_SFLOAT;
	attribDesc[1].offset = sizeof(glm::vec4);

	attribDesc[2].binding = 1;
	attribDesc[2].location = 2;
	attribDesc[2].format = RESOURCE_FORMAT_R32G32B32_SFLOAT;
	attribDesc[2].offset = 0;

	PipelineVertexInputInfo vertexInput = {};
	vertexInput.vertexInputBindings = bindingDesc;
	vertexInput.vertexInputAttribs = attribDesc;

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

	info.inputPushConstants = {0, 0};
	info.inputSetLayouts = {};

	gfxPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);
}

void VertexIndexBufferTest::createBuffers()
{
	glm::vec4 positions0[] = {
		glm::vec4(-0.5f, 0.8f, 0.0f, 1.0f), glm::vec4(1, 0, 0, 0),
		glm::vec4(-0.2f, -0.8f, 0.0f, 1.0f), glm::vec4(0, 1, 0, 0),
		glm::vec4(-0.8f, -0.8f, 0.0f, 1.0f), glm::vec4(0, 0, 1, 0)
	};

	glm::vec4 positions1[] = {
		glm::vec4(0.5f, 0.8f, 0.0f, 1.0f), glm::vec4(1, 0, 0, 0),
		glm::vec4(0.8f, -0.8f, 0.0f, 1.0f), glm::vec4(0, 1, 0, 0),
		glm::vec4(0.2f, -0.8f, 0.0f, 1.0f), glm::vec4(0, 0, 1, 0)
	};

	glm::vec4 colors[] = {
		glm::vec4(0, 1, 0, 0),
		glm::vec4(0, 0, 1, 0),
		glm::vec4(1, 0, 0, 0)
	};

	uint16_t indexes[] = {
		0, 1, 2
	};

	positionBuffers[0] = renderer->createBuffer(sizeof(positions0), BUFFER_USAGE_VERTEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);
	positionBuffers[1] = renderer->createBuffer(sizeof(positions1), BUFFER_USAGE_VERTEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);
	colorBuffer = renderer->createBuffer(sizeof(colors), BUFFER_USAGE_VERTEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);
	indexBuffer = renderer->createBuffer(sizeof(indexes), BUFFER_USAGE_INDEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);

	StagingBuffer stagingBuffer0 = renderer->createAndFillStagingBuffer(sizeof(positions0), positions0);
	StagingBuffer stagingBuffer1 = renderer->createAndFillStagingBuffer(sizeof(positions1), positions1);
	StagingBuffer stagingBuffer2 = renderer->createAndFillStagingBuffer(sizeof(colors), colors);
	StagingBuffer stagingBuffer3 = renderer->createAndFillStagingBuffer(sizeof(indexes), indexes);

	Fence tempFence = renderer->createFence();

	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	cmdBuffer->stageBuffer(stagingBuffer0, positionBuffers[0]);
	cmdBuffer->stageBuffer(stagingBuffer1, positionBuffers[1]);
	cmdBuffer->stageBuffer(stagingBuffer2, colorBuffer);
	cmdBuffer->stageBuffer(stagingBuffer3, indexBuffer);

	cmdBuffer->endCommands();

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 10);

	cmdPool->resetCommandPoolAndFreeCommandBuffer(cmdBuffer);
	renderer->destroyStagingBuffer(stagingBuffer0);
	renderer->destroyStagingBuffer(stagingBuffer1);
	renderer->destroyStagingBuffer(stagingBuffer2);
	renderer->destroyStagingBuffer(stagingBuffer3);
	renderer->destroyFence(tempFence);
}