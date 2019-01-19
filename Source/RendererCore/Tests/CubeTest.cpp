#include "RendererCore/Tests/CubeTest.h"

#include <RendererCore/Renderer.h>

#include <GLFW/glfw3.h>

/*


*/

CubeTest::CubeTest(Renderer *rendererPtr)
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
		test.addColorOutput("testOut", testOut, true, {0.75, 0.75, 0.1, 1});

		test.setInitFunction(std::bind(&CubeTest::passInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&CubeTest::passRender, this, std::placeholders::_1, std::placeholders::_2));

		gfxGraph->addNamedSize("swapchain", glm::uvec3(1920, 1080, 1));
		gfxGraph->setFrameGraphOutput("testOut");

		double sT = glfwGetTime();
		gfxGraph->build();
		Log::get()->info("RenderGraph build took {} ms", (glfwGetTime() - sT) * 1000.0);
	}

	createBuffers();
}

CubeTest::~CubeTest()
{
	renderer->destroyBuffer(cubeBuffer0);
	renderer->destroyBuffer(cubeBuffer1);

	renderer->destroySampler(renderTargetSampler);

	renderer->destroyPipeline(gfxPipeline);

	renderer->destroyCommandPool(cmdPool);

	renderer->destroyRenderGraph(gfxGraph);
}

void CubeTest::passInit(const RenderGraphInitFunctionData &data)
{
	if (gfxPipeline != nullptr)
		renderer->destroyPipeline(gfxPipeline);

	createPipeline(data);
}

void CubeTest::passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
	cmdBuffer->bindVertexBuffers(0, {cubeBuffer0, cubeBuffer1}, {0, 0});
	cmdBuffer->draw(3);
}

void CubeTest::render()
{
	renderDoneSemaphore = gfxGraph->execute();
}

void CubeTest::createBuffers()
{
	glm::vec4 buffer0[] = {
		glm::vec4(0.0f, 0.8f, 0.0f, 0.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(0.8f, -0.8f, 0.0f, 0.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(-0.8f, -0.8f, 0.0f, 0.0f), glm::vec4(0, 0, 1, 0.0f)
	};

	glm::vec4 buffer1[] = {
		glm::vec4(0.0f, -0.8f, 0.0f, 0.0f),
		glm::vec4(0.8f, 0.8f, 0.0f, 0.0f),
		glm::vec4(-0.8f, -0.8f, 0.0f, 0.0f)
	};

	cubeBuffer0 = renderer->createBuffer(sizeof(buffer0), BUFFER_USAGE_VERTEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);
	cubeBuffer1 = renderer->createBuffer(sizeof(buffer1), BUFFER_USAGE_VERTEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);

	StagingBuffer stagingBuffer0 = renderer->createAndFillStagingBuffer(sizeof(buffer0), buffer0);
	StagingBuffer stagingBuffer1 = renderer->createAndFillStagingBuffer(sizeof(buffer1), buffer1);

	Fence tempFence = renderer->createFence();

	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	cmdBuffer->stageBuffer(stagingBuffer0, cubeBuffer0);
	cmdBuffer->stageBuffer(stagingBuffer1, cubeBuffer1);

	cmdBuffer->endCommands();

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 10);

	cmdPool->resetCommandPoolAndFreeCommandBuffer(cmdBuffer);
	renderer->destroyStagingBuffer(stagingBuffer0);
	renderer->destroyStagingBuffer(stagingBuffer1);
	renderer->destroyFence(tempFence);
}

void CubeTest::createPipeline(const RenderGraphInitFunctionData &data)
{
	ShaderModule vertShader = renderer->createShaderModule("GameData/shaders/tests/cube_test.hlsl", SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_HLSL, "CubeTestVS");
	ShaderModule fragShader = renderer->createShaderModule("GameData/shaders/tests/cube_test.hlsl", SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_HLSL, "CubeTestFS");

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
	attribDesc[2].format = RESOURCE_FORMAT_R32G32_SFLOAT;
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

	info.inputPushConstantRanges = {};
	info.inputSetLayouts = {{
			//{0, DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, SHADER_STAGE_FRAGMENT_BIT},
		}};

	gfxPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);
}