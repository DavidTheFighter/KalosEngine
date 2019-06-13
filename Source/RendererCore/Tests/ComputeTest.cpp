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

	testStorageBuffer = renderer->createBuffer(256, BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT, BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL, MEMORY_USAGE_GPU_ONLY);
	StagingBuffer storageStagingBuffer = renderer->createAndFillStagingBuffer(256, storageData);

	Fence tempFence = renderer->createFence();

	CommandBuffer cmdBuffer = cmdPool->allocateCommandBuffer();
	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	cmdBuffer->stageBuffer(storageStagingBuffer, testStorageBuffer);

	ResourceBarrier storageBufferBarrier = {};
	storageBufferBarrier.barrierType = RESOURCE_BARRIER_TYPE_BUFFER_TRANSITION;
	storageBufferBarrier.bufferTransition.buffer = testStorageBuffer;
	storageBufferBarrier.bufferTransition.oldLayout = BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL;
	storageBufferBarrier.bufferTransition.newLayout = BUFFER_LAYOUT_GENERAL;

	cmdBuffer->resourceBarriers({storageBufferBarrier});

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

		auto &bufferGen = gfxGraph->addRenderPass("bufferGen", RENDER_GRAPH_PIPELINE_TYPE_COMPUTE);
		bufferGen.addStorageBuffer("graphStorageBuffer", 64 * 64 * sizeof(float), BUFFER_USAGE_STORAGE_BUFFER_BIT, false, true);

		bufferGen.setRenderFunction(std::bind(&ComputeTest::bufferGenPassRender, this, std::placeholders::_1, std::placeholders::_2));

		auto &test = gfxGraph->addRenderPass("test", RENDER_GRAPH_PIPELINE_TYPE_COMPUTE);
		test.addStorageTexture("testOut", testOut, false, true);
		test.addStorageBuffer("graphStorageBuffer", 64 * 64 * sizeof(float), BUFFER_USAGE_STORAGE_BUFFER_BIT, true, false);

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
	renderer->destroyPipeline(bufferGenPipeline);

	renderer->destroyRenderGraph(gfxGraph);
}

void ComputeTest::passInit(const RenderGraphInitFunctionData &data)
{
	if (compPipeline != nullptr)
		renderer->destroyPipeline(compPipeline);

	createPipeline(data);
	createBufferGenPipeline(data);
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

	DescriptorBufferInfo graphStorageBufferInfo = {};
	graphStorageBufferInfo.buffer = data.graphBuffers.at("graphStorageBuffer");
	graphStorageBufferInfo.offset = 0;
	graphStorageBufferInfo.range = 64 * 64 * sizeof(float);

	std::vector<DescriptorWriteInfo> writes(3);
	writes[0].descriptorType = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	writes[0].dstBinding = 0;
	writes[0].dstArrayElement = 0;
	writes[0].textureInfo = {testOutputInfo};

	writes[1].descriptorType = DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[1].dstBinding = 1;
	writes[1].dstArrayElement = 0;
	writes[1].bufferInfo = {testStorageBufferInfo};

	writes[2].descriptorType = DESCRIPTOR_TYPE_STORAGE_BUFFER;
	writes[2].dstBinding = 2;
	writes[2].dstArrayElement = 0;
	writes[2].bufferInfo = {graphStorageBufferInfo};

	renderer->writeDescriptorSets(descSet, writes);
}

void ComputeTest::passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_COMPUTE, compPipeline);
	cmdBuffer->bindDescriptorSets(PIPELINE_BIND_POINT_COMPUTE, 0, {descSet});
	cmdBuffer->dispatch(gfxGraph->getNamedSize("swapchain").x / 8, gfxGraph->getNamedSize("swapchain").y / 8, 1);
}

void ComputeTest::bufferGenPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_COMPUTE, bufferGenPipeline);
	cmdBuffer->bindDescriptorSets(PIPELINE_BIND_POINT_COMPUTE, 0, {descSet});
	cmdBuffer->dispatch(8, 8, 1);
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

	DescriptorSetBinding outTextureBinding = {};
	outTextureBinding.binding = 0;
	outTextureBinding.arrayCount = 1;
	outTextureBinding.type = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	outTextureBinding.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetBinding testStorageBinding = {};
	testStorageBinding.binding = 1;
	testStorageBinding.arrayCount = 1;
	testStorageBinding.type = DESCRIPTOR_TYPE_STORAGE_BUFFER;
	testStorageBinding.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetBinding graphStorageBufferBinding = {};
	graphStorageBufferBinding.binding = 2;
	graphStorageBufferBinding.arrayCount = 1;
	graphStorageBufferBinding.type = DESCRIPTOR_TYPE_STORAGE_BUFFER;
	graphStorageBufferBinding.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetLayoutDescription set0 = {};
	set0.bindings = {outTextureBinding, testStorageBinding, graphStorageBufferBinding};

	ComputePipelineInfo info = {};
	info.shader = compShaderStage;
	info.inputPushConstants = {0, 0};
	info.inputSetLayouts = {set0};

	compPipeline = renderer->createComputePipeline(info);

	renderer->destroyShaderModule(compShaderStage.shaderModule);

	descPool = renderer->createDescriptorPool(set0, 1);

	descSet = descPool->allocateDescriptorSet();
}

void ComputeTest::createBufferGenPipeline(const RenderGraphInitFunctionData &data)
{
	ShaderModule compShader = renderer->createShaderModule("GameData/shaders/tests/compute_test.hlsl", SHADER_STAGE_COMPUTE_BIT, SHADER_LANGUAGE_HLSL, "ComputeBufferGenTestCS");

	PipelineShaderStage compShaderStage = {};
	compShaderStage.shaderModule = compShader;

	DescriptorSetBinding outTextureBinding = {};
	outTextureBinding.binding = 0;
	outTextureBinding.arrayCount = 1;
	outTextureBinding.type = DESCRIPTOR_TYPE_STORAGE_TEXTURE;
	outTextureBinding.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetBinding testStorageBinding = {};
	testStorageBinding.binding = 1;
	testStorageBinding.arrayCount = 1;
	testStorageBinding.type = DESCRIPTOR_TYPE_STORAGE_BUFFER;
	testStorageBinding.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetBinding graphStorageBufferBinding = {};
	graphStorageBufferBinding.binding = 2;
	graphStorageBufferBinding.arrayCount = 1;
	graphStorageBufferBinding.type = DESCRIPTOR_TYPE_STORAGE_BUFFER;
	graphStorageBufferBinding.stageAccessMask = SHADER_STAGE_COMPUTE_BIT;

	DescriptorSetLayoutDescription set0 = {};
	set0.bindings = {outTextureBinding, testStorageBinding, graphStorageBufferBinding};

	ComputePipelineInfo info = {};
	info.shader = compShaderStage;
	info.inputPushConstants = {0, 0};
	info.inputSetLayouts = {set0};

	bufferGenPipeline = renderer->createComputePipeline(info);

	renderer->destroyShaderModule(compShaderStage.shaderModule);
}