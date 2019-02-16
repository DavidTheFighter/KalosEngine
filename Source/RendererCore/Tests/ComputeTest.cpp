#include "RendererCore/Tests/ComputeTest.h"

#include <RendererCore/Renderer.h>

/*



*/

ComputeTest::ComputeTest(Renderer *rendererPtr)
{
	renderer = rendererPtr;

	compPipeline = nullptr;
	cmdPool = renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT);

	renderTargetSampler = renderer->createSampler();

	float storageData[64];
	storageData[0] = 1.0f;
	storageData[1] = 0.5f;

	testStorageBuffer = renderer->createBuffer(256, BUFFER_USAGE_STORAGE_BUFFER, true, false, MEMORY_USAGE_GPU_ONLY);
	StagingBuffer storageStagingBuffer = renderer->createAndFillStagingBuffer(256, storageData);

	Fence tempFence = renderer->createFence();

	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	cmdBuffer->stageBuffer(storageStagingBuffer, testStorageBuffer);

	cmdBuffer->endCommands();

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 5);

	cmdPool->resetCommandPoolAndFreeCommandBuffer(cmdBuffer);
	renderer->destroyStagingBuffer(storageStagingBuffer);
	renderer->destroyFence(tempFence);

	{
		gfxGraph = renderer->createRenderGraph();

		RenderPassAttachment testOut;
		testOut.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
		testOut.namedRelativeSize = "swapchain";

		auto &test = gfxGraph->addRenderPass("test", RENDER_GRAPH_PIPELINE_TYPE_COMPUTE);
		test.addStorageTexture("testOut", testOut, false, true);

		test.setInitFunction(std::bind(&ComputeTest::passInit, this, std::placeholders::_1));
		test.setDescriptorUpdateFunction(std::bind(&ComputeTest::passDescUpdate, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&ComputeTest::passRender, this, std::placeholders::_1, std::placeholders::_2));

		gfxGraph->addNamedSize("swapchain", glm::uvec3(1920, 1080, 1));
		gfxGraph->setRenderGraphOutput("testOut");

		gfxGraph->build();
	}
}

ComputeTest::~ComputeTest()
{
	renderer->destroySampler(renderTargetSampler);

	renderer->destroyBuffer(testStorageBuffer);

	renderer->destroyDescriptorPool(descPool);
	renderer->destroyCommandPool(cmdPool);

	renderer->destroyPipeline(compPipeline);

	renderer->destroyRenderGraph(gfxGraph);
}

void ComputeTest::passInit(const RenderGraphInitFunctionData &data)
{
	if (compPipeline != nullptr)
		renderer->destroyPipeline(compPipeline);

	createPipeline(data);
}

void ComputeTest::passDescUpdate(const RenderGraphDescriptorUpdateFunctionData& data)
{
	DescriptorTextureInfo testOutputInfo = {};
	testOutputInfo.layout = TEXTURE_LAYOUT_GENERAL;
	testOutputInfo.view = data.graphTextureViews.at("testOut");

	DescriptorBufferInfo testStorageBufferInfo = {};
	testStorageBufferInfo.buffer = testStorageBuffer;
	testStorageBufferInfo.offset = 0;
	testStorageBufferInfo.range = 256;

	std::vector<DescriptorWriteInfo> writes(2);
	writes[0].descriptorType = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	writes[0].dstBinding = 0;
	writes[0].sampledTextureInfo = testOutputInfo;

	writes[1].descriptorType = DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].dstBinding = 0;
	writes[1].bufferInfo = testStorageBufferInfo;

	renderer->writeDescriptorSets(descSet, writes);
}

void ComputeTest::passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_COMPUTE, compPipeline);
	cmdBuffer->bindDescriptorSets(PIPELINE_BIND_POINT_COMPUTE, 0, {descSet});
	cmdBuffer->dispatch(gfxGraph->getNamedSize("swapchain").x / 8, gfxGraph->getNamedSize("swapchain").y / 8, 1);
}

void ComputeTest::render()
{
	renderDoneSemaphore = gfxGraph->execute(true);
}

void ComputeTest::createPipeline(const RenderGraphInitFunctionData &data)
{
	ShaderModule compShader = renderer->createShaderModule("GameData/shaders/tests/compute_test.hlsl", SHADER_STAGE_COMPUTE_BIT, SHADER_LANGUAGE_HLSL, "ComputeTestCS");

	PipelineShaderStage compShaderStage = {};
	compShaderStage.shaderModule = compShader;

	DescriptorSetLayoutDescription set0 = {};
	set0.storageBufferDescriptorCount = 1;
	set0.storageTextureDescriptorCount = 1;
	set0.storageBufferBindingsShaderStageAccess = {SHADER_STAGE_COMPUTE_BIT};
	set0.storageTextureBindingsShaderStageAccess = {SHADER_STAGE_COMPUTE_BIT};

	ComputePipelineInfo info = {};
	info.shader = compShaderStage;
	info.inputPushConstants = {0, 0};
	info.inputSetLayouts = {set0};

	compPipeline = renderer->createComputePipeline(info);

	renderer->destroyShaderModule(compShaderStage.shaderModule);

	descPool = renderer->createDescriptorPool(set0, 1);

	descSet = descPool->allocateDescriptorSet();
}