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

		RenderPassAttachment testOutDepth;
		testOutDepth.format = RESOURCE_FORMAT_D16_UNORM;
		testOutDepth.namedRelativeSize = "swapchain";

		auto &test = gfxGraph->addRenderPass("test", RG_PIPELINE_GRAPHICS);
		test.addColorOutput("testOut", testOut, true, {0.75, 0.75, 0.1, 1});
		test.setDepthStencilOutput("testOutDepth", testOutDepth, true, {1, 0});

		test.setInitFunction(std::bind(&CubeTest::passInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&CubeTest::passRender, this, std::placeholders::_1, std::placeholders::_2));

		gfxGraph->addNamedSize("swapchain", glm::uvec3(1920, 1080, 1));
		gfxGraph->setFrameGraphOutput("testOut");

		double sT = glfwGetTime();
		gfxGraph->build();
		Log::get()->info("RenderGraph build took {} ms", (glfwGetTime() - sT) * 1000.0);
	}

	createBuffers();

	rotateCounter = 0.0f;
}

CubeTest::~CubeTest()
{
	renderer->destroyBuffer(cubeBuffer0);
	renderer->destroyBuffer(cubeBuffer1);
	renderer->destroyBuffer(cubeIndexBuffer);

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
	glm::mat4 camModlMat = glm::rotate(glm::mat4(1), rotateCounter, glm::vec3(0, 1, 0));
	glm::mat4 camViewMat = glm::lookAt(glm::vec3(7.5f, 3.0f, 0.0f), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
	glm::mat4 camProjMat = glm::perspective<float>(60.0f * (M_PI / 180.0f), 16 / 9.0f, 0.1f, 100.0f);
	glm::mat4 camMVPMat = camProjMat * camViewMat * camModlMat;

	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_GRAPHICS, gfxPipeline);
	cmdBuffer->bindIndexBuffer(cubeIndexBuffer, 0);
	cmdBuffer->bindVertexBuffers(0, {cubeBuffer0, cubeBuffer1}, {0, 0});
	cmdBuffer->pushConstants(0, sizeof(camMVPMat), &camMVPMat[0][0]);

	cmdBuffer->drawIndexed(36);
}

void CubeTest::render()
{
	rotateCounter += 1 / 60.0f;

	renderDoneSemaphore = gfxGraph->execute();
}

void CubeTest::createBuffers()
{
	glm::vec4 buffer0[] = {
		// -x side
		glm::vec4(-1.0f,-1.0f,-1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(-1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),
		glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(-1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(-1.0f,-1.0f,-1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),

		// -z side
		glm::vec4(-1.0f,-1.0f,-1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(1.0f, 1.0f, -1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),
		glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f, 1.0f, -1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),

		// -y side
		glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f, -1.0f, 1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),
		glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(1.0f, -1.0f, 1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),

		// +y side
		glm::vec4(-1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),
		glm::vec4(-1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),

		// +x side
		glm::vec4(1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),
		glm::vec4(1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(1.0f,-1.0f,-1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),

		// +z side
		glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(-1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f),
		glm::vec4(-1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(1, 0, 0, 0.0f),
		glm::vec4(1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(0, 1, 0, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0, 0, 1, 0.0f)
	};

	glm::vec4 buffer1[] = {
		// -x side
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),

		// -z side
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),

		// -y side
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),

		// +y side
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),

		// +x side
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),

		// +z side
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)
	};

	uint16_t indexBufferData[] = {
		0, 1, 2, 3, 4, 5,
		6, 7, 8, 9, 10, 11,
		12, 13, 14, 15, 16, 17,
		18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29,
		30, 31, 32, 33, 34, 35
	};

	cubeBuffer0 = renderer->createBuffer(sizeof(buffer0), BUFFER_USAGE_VERTEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);
	cubeBuffer1 = renderer->createBuffer(sizeof(buffer1), BUFFER_USAGE_VERTEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);
	cubeIndexBuffer = renderer->createBuffer(sizeof(indexBufferData), BUFFER_USAGE_INDEX_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);

	StagingBuffer stagingBuffer0 = renderer->createAndFillStagingBuffer(sizeof(buffer0), buffer0);
	StagingBuffer stagingBuffer1 = renderer->createAndFillStagingBuffer(sizeof(buffer1), buffer1);
	StagingBuffer stagingIndexBuffer = renderer->createAndFillStagingBuffer(sizeof(indexBufferData), indexBufferData);

	Fence tempFence = renderer->createFence();

	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	cmdBuffer->stageBuffer(stagingBuffer0, cubeBuffer0);
	cmdBuffer->stageBuffer(stagingBuffer1, cubeBuffer1);
	cmdBuffer->stageBuffer(stagingIndexBuffer, cubeIndexBuffer);

	cmdBuffer->endCommands();

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 10);

	cmdPool->resetCommandPoolAndFreeCommandBuffer(cmdBuffer);
	renderer->destroyStagingBuffer(stagingBuffer0);
	renderer->destroyStagingBuffer(stagingBuffer1);
	renderer->destroyStagingBuffer(stagingIndexBuffer);
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
	rastInfo.cullMode = POLYGON_CULL_MODE_BACK;
	rastInfo.polygonMode = POLYGON_MODE_FILL;
	rastInfo.enableOutOfOrderRasterization = false;

	PipelineDepthStencilInfo depthInfo = {};
	depthInfo.enableDepthTest = true;
	depthInfo.enableDepthWrite = true;
	depthInfo.depthCompareOp = COMPARE_OP_LESS;

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
	info.inputSetLayouts = {{
			//{0, DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1, SHADER_STAGE_FRAGMENT_BIT},
		}};

	gfxPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);
}