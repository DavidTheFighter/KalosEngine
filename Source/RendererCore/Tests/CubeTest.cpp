#include "RendererCore/Tests/CubeTest.h"

#include <RendererCore/Renderer.h>

#include <GLFW/glfw3.h>

/*

This test should produce a rotating cube, colored blue with red star-like patterns on each face, on a white background.

Specifically this tests:
- Vertex and index buffers
- Basic descriptor usage (1 cbv, 1 texture, 1 sampler)
- Depth buffering
- Push constants (only vertex stage)
- Texture sampling, uploading/staging
- Buffer uploading/staging
- Propery face culling order
- Viewports/scissors, essentially everythign the triangle test does

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

		auto &test = gfxGraph->addRenderPass("test", RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS);
		test.addColorAttachmentOutput("testOut", testOut, true, {1.0f, 1.0f, 1.0f, 1.0f});
		test.setDepthStencilAttachmentOutput("testOutDepth", testOutDepth, true, {1, 0});

		test.setInitFunction(std::bind(&CubeTest::passInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&CubeTest::passRender, this, std::placeholders::_1, std::placeholders::_2));

		gfxGraph->addNamedSize("swapchain", glm::uvec3(1920, 1080, 1));
		gfxGraph->setRenderGraphOutput("testOut");

		double sT = glfwGetTime();
		gfxGraph->build();
		Log::get()->info("RenderGraph build took {} ms", (glfwGetTime() - sT) * 1000.0);
	}

	createBuffers();
	createTextures();

	cubeTexSampler = renderer->createSampler(SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

	rotateCounter = 0.0f;

	descSet = cubeDescPool->allocateDescriptorSet();

	std::vector<DescriptorWriteInfo> writes(3);
	writes[0].dstBinding = 0;
	writes[0].descriptorType = DESCRIPTOR_TYPE_CONSTANT_BUFFER;
	writes[0].bufferInfo = {cubeCBuffer, 0, sizeof(glm::mat4)};

	writes[1].dstBinding = 0;
	writes[1].descriptorType = DESCRIPTOR_TYPE_SAMPLER;
	writes[1].samplerInfo = {cubeTexSampler};

	writes[2].dstBinding = 0;
	writes[2].descriptorType = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
	writes[2].sampledTextureInfo = {cubeTestTextureView, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

	renderer->writeDescriptorSets(descSet, writes);
}

CubeTest::~CubeTest()
{
	renderer->destroyBuffer(cubeBuffer);
	renderer->destroyBuffer(cubeIndexBuffer);
	renderer->destroyBuffer(cubeCBuffer);

	renderer->destroySampler(renderTargetSampler);
	renderer->destroySampler(cubeTexSampler);
	renderer->destroyTextureView(cubeTestTextureView);
	renderer->destroyTexture(cubeTestTexture);

	renderer->destroyPipeline(gfxPipeline);

	renderer->destroyDescriptorPool(cubeDescPool);
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
	cmdBuffer->bindVertexBuffers(0, {cubeBuffer}, {0});
	cmdBuffer->pushConstants(0, sizeof(camMVPMat), &camMVPMat[0][0]);
	cmdBuffer->bindDescriptorSets(PIPELINE_BIND_POINT_GRAPHICS, 0, {descSet});

	cmdBuffer->drawIndexed(36);
}

void CubeTest::render()
{
	rotateCounter += 1 / 60.0f;

	renderDoneSemaphore = gfxGraph->execute(true);
}

void CubeTest::createBuffers()
{
	glm::vec4 cubeBufferData[] = {
		// -x side
		glm::vec4(-1.0f,-1.0f,-1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f,-1.0f,-1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),

		// -z side
		glm::vec4(-1.0f,-1.0f,-1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, -1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f, -1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, -1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),

		// -y side
		glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, -1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, -1.0f, -1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, -1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, -1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),

		// +y side
		glm::vec4(-1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),

		// +x side
		glm::vec4(1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f,-1.0f,-1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f,-1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),

		// +z side
		glm::vec4(-1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
		glm::vec4(-1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f,-1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
		glm::vec4(1.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 0.0f),
	};

	uint16_t indexBufferData[] = {
		0, 1, 2, 3, 4, 5,
		6, 7, 8, 9, 10, 11,
		12, 13, 14, 15, 16, 17,
		18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29,
		30, 31, 32, 33, 34, 35
	};

	glm::mat4 cbufferData[] = {glm::mat4(1)};

	cubeBuffer = renderer->createBuffer(sizeof(cubeBufferData), BUFFER_USAGE_VERTEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT, BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL, MEMORY_USAGE_GPU_ONLY);
	cubeIndexBuffer = renderer->createBuffer(sizeof(indexBufferData), BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT, BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL, MEMORY_USAGE_GPU_ONLY);
	cubeCBuffer = renderer->createBuffer(sizeof(cbufferData), BUFFER_USAGE_CONSTANT_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT, BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL, MEMORY_USAGE_GPU_ONLY);

	StagingBuffer stagingCubeBuffer = renderer->createAndFillStagingBuffer(sizeof(cubeBufferData), cubeBufferData);
	StagingBuffer stagingIndexBuffer = renderer->createAndFillStagingBuffer(sizeof(indexBufferData), indexBufferData);
	StagingBuffer stagingCubeCBuffer = renderer->createAndFillStagingBuffer(sizeof(cbufferData), cbufferData);

	Fence tempFence = renderer->createFence();

	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	cmdBuffer->stageBuffer(stagingCubeBuffer, cubeBuffer);
	cmdBuffer->stageBuffer(stagingIndexBuffer, cubeIndexBuffer);
	cmdBuffer->stageBuffer(stagingCubeCBuffer, cubeCBuffer);

	ResourceBarrier cubeBufferBarrier = {};
	cubeBufferBarrier.barrierType = RESOURCE_BARRIER_TYPE_BUFFER_TRANSITION;
	cubeBufferBarrier.bufferTransition.buffer = cubeBuffer;
	cubeBufferBarrier.bufferTransition.oldLayout = BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL;
	cubeBufferBarrier.bufferTransition.newLayout = BUFFER_LAYOUT_VERTEX_BUFFER;

	ResourceBarrier cubeIndexBufferBarrier = {};
	cubeIndexBufferBarrier.barrierType = RESOURCE_BARRIER_TYPE_BUFFER_TRANSITION;
	cubeIndexBufferBarrier.bufferTransition.buffer = cubeIndexBuffer;
	cubeIndexBufferBarrier.bufferTransition.oldLayout = BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL;
	cubeIndexBufferBarrier.bufferTransition.newLayout = BUFFER_LAYOUT_INDEX_BUFFER;

	ResourceBarrier cubeCBufferBarrier = {};
	cubeCBufferBarrier.barrierType = RESOURCE_BARRIER_TYPE_BUFFER_TRANSITION;
	cubeCBufferBarrier.bufferTransition.buffer = cubeCBuffer;
	cubeCBufferBarrier.bufferTransition.oldLayout = BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL;
	cubeCBufferBarrier.bufferTransition.newLayout = BUFFER_LAYOUT_CONSTANT_BUFFER;

	cmdBuffer->resourceBarriers({cubeBufferBarrier, cubeIndexBufferBarrier, cubeCBufferBarrier});

	cmdBuffer->endCommands();

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 5);

	cmdPool->resetCommandPoolAndFreeCommandBuffer(cmdBuffer);
	renderer->destroyStagingBuffer(stagingCubeBuffer);
	renderer->destroyStagingBuffer(stagingIndexBuffer);
	renderer->destroyStagingBuffer(stagingCubeCBuffer);
	renderer->destroyFence(tempFence);
}

void CubeTest::createTextures()
{
	const uint32_t testTextureWidth = 256;
	const uint32_t testTextureHeight = 256;

	cubeTestTexture = renderer->createTexture({testTextureWidth, testTextureHeight, 1}, RESOURCE_FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT, MEMORY_USAGE_GPU_ONLY);
	cubeTestTextureView = renderer->createTextureView(cubeTestTexture);

	uint8_t textureData[testTextureWidth][testTextureHeight][4];

	for (uint32_t x = 0; x < testTextureWidth; x++)
	{
		for (uint32_t y = 0; y < testTextureHeight; y++)
		{
			textureData[x][y][0] = (uint32_t) (std::min(std::sin((x / float(testTextureWidth - 1)) * M_PI), std::sin((y / float(testTextureHeight - 1)) * M_PI)) * 255.0f);
			textureData[x][y][1] = 0;// (uint32_t) (std::min(std::max(0.0, std::sin((x / float(testTextureWidth - 1)) * M_PI * 16.0f)), std::max(0.0, std::sin((y / float(testTextureHeight - 1)) * M_PI * 16.0f))) * 255.0f);
			textureData[x][y][2] = 255 - textureData[x][y][0];
			textureData[x][y][3] = 255;
		}
	}

	StagingTexture stagingTexture = renderer->createStagingTexture({testTextureWidth, testTextureHeight, 1}, RESOURCE_FORMAT_R8G8B8A8_UNORM, 1, 1);
	renderer->fillStagingTextureSubresource(stagingTexture, textureData, 0, 0);

	Fence tempFence = renderer->createFence();

	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	ResourceBarrier prestageTextureBarrier = {};
	prestageTextureBarrier.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
	prestageTextureBarrier.textureTransition.texture = cubeTestTexture;
	prestageTextureBarrier.textureTransition.oldLayout = TEXTURE_LAYOUT_INITIAL_STATE;
	prestageTextureBarrier.textureTransition.newLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
	prestageTextureBarrier.textureTransition.subresourceRange = {0, 1, 0, 1};

	ResourceBarrier poststageTextureBarrier = {};
	poststageTextureBarrier.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
	poststageTextureBarrier.textureTransition.texture = cubeTestTexture;
	poststageTextureBarrier.textureTransition.oldLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
	poststageTextureBarrier.textureTransition.newLayout = TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	poststageTextureBarrier.textureTransition.subresourceRange = {0, 1, 0, 1};

	cmdBuffer->resourceBarriers({prestageTextureBarrier});
	cmdBuffer->stageTextureSubresources(stagingTexture, cubeTestTexture, {0, 1, 0, 1});
	cmdBuffer->resourceBarriers({poststageTextureBarrier});

	cmdBuffer->endCommands();

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 5);

	cmdPool->resetCommandPoolAndFreeCommandBuffer(cmdBuffer);
	renderer->destroyStagingTexture(stagingTexture);
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

	std::vector<VertexInputBinding> bindingDesc = std::vector<VertexInputBinding>(1);
	bindingDesc[0].binding = 0;
	bindingDesc[0].stride = sizeof(glm::vec4) * 2;
	bindingDesc[0].inputRate = VERTEX_INPUT_RATE_VERTEX;

	std::vector<VertexInputAttribute> attribDesc = std::vector<VertexInputAttribute>(2);
	attribDesc[0].binding = 0;
	attribDesc[0].location = 0;
	attribDesc[0].format = RESOURCE_FORMAT_R32G32B32_SFLOAT;
	attribDesc[0].offset = 0;

	attribDesc[1].binding = 0;
	attribDesc[1].location = 1;
	attribDesc[1].format = RESOURCE_FORMAT_R32G32_SFLOAT;
	attribDesc[1].offset = sizeof(glm::vec4);

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

	DescriptorSetLayoutDescription set0 = {};
	set0.samplerDescriptorCount = 1;
	set0.constantBufferDescriptorCount = 1;
	set0.inputAttachmentDescriptorCount = 0;
	set0.textureDescriptorCount = 1;
	set0.samplerBindingsShaderStageAccess = {SHADER_STAGE_FRAGMENT_BIT};
	set0.constantBufferBindingsShaderStageAccess = {SHADER_STAGE_VERTEX_BIT};
	set0.inputAttachmentBindingsShaderStageAccess = {};
	set0.textureBindingsShaderStageAccess = {SHADER_STAGE_FRAGMENT_BIT};

	info.inputPushConstants = {sizeof(glm::mat4), SHADER_STAGE_VERTEX_BIT};
	info.inputSetLayouts = {set0};

	gfxPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);

	cubeDescPool = renderer->createDescriptorPool(set0, 1);
}