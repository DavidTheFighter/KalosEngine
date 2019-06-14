#include "KalosEngine.h"

#include <chrono>

#include <Resources/FileLoader.h>

#include <GLFW/glfw3.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#include <nuklear.h>

#include <Renderer/NuklearGUIRenderer.h>

#include <RendererCore/Tests/RenderTestHandler.h>

KalosEngine::KalosEngine(const std::vector<std::string> &launchArgs, RendererBackend rendererBackendType, uint32_t engineUpdateFrequencyCap)
{
	this->launchArgs = launchArgs;
	this->rendererBackendType = rendererBackendType;
	this->updateFrequencyCap = engineUpdateFrequencyCap;

	currentGameStateOutput = nullptr;
	currentEngineTextureOutput = nullptr;
	lastUpdateTime = getTime();

	engineIsRunning = true;
	
	mainWindow = std::unique_ptr<Window>(new Window(rendererBackendType));
	mainWindow->initWindow(0, 0, APP_NAME);

	mainWindowSize = glm::uvec2(mainWindow->getWidth(), mainWindow->getHeight());

	RendererAllocInfo renderAlloc = {};
	renderAlloc.backend = rendererBackendType;
	renderAlloc.launchArgs = launchArgs;
	renderAlloc.mainWindow = mainWindow.get();

	renderer = std::unique_ptr<Renderer>(Renderer::allocateRenderer(renderAlloc));

	initCmdPool = renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT);
	swapchainSampler = renderer->createSampler();

	initColorTextures();
	setupDebugInfoRenderGraph();

	doingRenderingTest = true;

	RendererTest currentRenderingTest;

	if (std::find(launchArgs.begin(), launchArgs.end(), "-triangle_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_TRIANGLE;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-vertex_index_buffer_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_VERTEX_INDEX_BUFFER;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-push_constants_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_PUSH_CONSTANTS;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-cube_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_CUBE;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-compute_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_COMPUTE;
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-msaa_test") != launchArgs.end())
		currentRenderingTest = RENDERER_TEST_MSAA;
	else
		doingRenderingTest = false;

	if (doingRenderingTest)
		renderTestHandler = std::unique_ptr<RenderTestHandler>(new RenderTestHandler(renderer.get(), mainWindow.get(), currentRenderingTest));

	worldManager = std::unique_ptr<WorldManager>(new WorldManager());
}

KalosEngine::~KalosEngine()
{
	renderer->waitForDeviceIdle();

	nuklearRenderer.reset();

	nk_free(nuklearCtx);
	delete nuklearCtx;

	renderer->destroyTexture(whiteTexture2D);
	renderer->destroyTexture(testFontAtlas);

	renderer->destroyTextureView(testFontAtlasView);
	renderer->destroyTextureView(whiteTexture2DView);

	renderer->destroyDescriptorPool(testFontAtlasDescriptrPool);
	renderer->destroyDescriptorPool(quadPassthroughDescriptorPool);

	renderer->destroyCommandPool(initCmdPool);

	renderer->destroyRenderGraph(debugInfoRenderGraph);

	renderer->destroyPipeline(debugInfoPassthroughPipeline);

	renderer->destroySampler(swapchainSampler);

	while (!gameStates.empty())
	{
		gameStates.back()->popped();
		gameStates.pop_back();
	}
}

void KalosEngine::setEngineMaxUpdateFrequency(uint32_t engineUpdateFrequencyCap)
{
	updateFrequencyCap = engineUpdateFrequencyCap;
}

void KalosEngine::handleEvents()
{
	mainWindow->pollEvents();

	if (mainWindow->userRequestedClose())
	{
		quit();

		return;
	}

	if (!gameStates.empty())
		gameStates.back()->handleEvents();
}

void KalosEngine::update()
{
	if (true)
	{
		double frameTimeTarget = 1 / double(updateFrequencyCap);

		if (getTime() - lastUpdateTime < frameTimeTarget)
		{
#ifdef __linux__
			usleep(uint32_t(std::max<double>(frameTimeTarget - (getTime() - lastUpdateTime) - 0.001, 0) * 1000000.0));
#elif defined(_WIN32)
			Sleep(DWORD(std::max<double>(frameTimeTarget - (getTime() - lastUpdateTime) - 0.001, 0) * 1000.0));
#endif
			while (getTime() - lastUpdateTime < frameTimeTarget)
			{
				// Busy wait for the rest of the time
			}
		}
	}

	float delta = getTime() - lastUpdateTime;

	glm::uvec2 lastMainWindowSize = mainWindowSize;
	mainWindowSize = glm::uvec2(mainWindow->getWidth(), mainWindow->getHeight());

	if (mainWindowSize != lastMainWindowSize)
	{
		renderer->recreateSwapchain(mainWindow.get());
	}

	lastUpdateTime = getTime();

	updateFunctionTimer += delta;
	if (updateFunctionTimer > 1.0f)
		updatePacedDelta = delta;

	updateFunctionTimer -= std::floor(updateFunctionTimer);

	nk_clear(nuklearCtx);

	struct nk_color table[NK_COLOR_COUNT];
	table[NK_COLOR_TEXT] = nk_rgba(255, 255, 255, 255);
	table[NK_COLOR_WINDOW] = nk_rgba(57, 67, 71, 0);
	nk_style_from_table(nuklearCtx, table);

	if (nk_begin(nuklearCtx, "DebugText", nk_rect(0, 0, 1920, 1080), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_NO_INPUT))
	{
		std::string rendererBackendStr = "???";

		switch (getRendererBackendType())
		{
		case RENDERER_BACKEND_VULKAN:
			rendererBackendStr = "Vulkan";
			break;
		case RENDERER_BACKEND_D3D12:
			rendererBackendStr = "D3D12";
			break;
		}

		char renderInfoCStr[512];
		sprintf(renderInfoCStr, "%s - Frametime: %.3f ms (%.1f FPS)", rendererBackendStr.c_str(), updatePacedDelta * 1000.0f, 1.0f / updatePacedDelta);

		nk_layout_row_static(nuklearCtx, 30, 1920, 1);
		nk_label(nuklearCtx, renderInfoCStr, NK_TEXT_ALIGN_LEFT);
	}
	nk_end(nuklearCtx);

	if (!gameStates.empty())
		gameStates.back()->update(delta);
}

void KalosEngine::render()
{
	if (doingRenderingTest)
	{
		renderTestHandler->render();
	}
	else if (!gameStates.empty())
	{
		gameStates.back()->render();

		if (currentGameStateOutput != gameStates.back()->getOutputTexture())
		{
			currentGameStateOutput = gameStates.back()->getOutputTexture();

			RendererDescriptorWriteInfo descWrite = {};
			descWrite.dstBinding = 1;
			descWrite.dstArrayElement = 0;
			descWrite.descriptorType = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
			descWrite.textureInfo = {{currentGameStateOutput, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};

			renderer->writeDescriptorSets(debugInfoPassthroughDescriptorSet, {descWrite});
		}

		if (currentEngineTextureOutput != debugInfoRenderGraph->getRenderGraphOutputTextureView())
		{
			currentEngineTextureOutput = debugInfoRenderGraph->getRenderGraphOutputTextureView();
			renderer->setSwapchainTexture(mainWindow.get(), currentEngineTextureOutput, swapchainSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		debugInfoRenderGraph->execute(false);

		renderer->waitForQueueIdle(QUEUE_TYPE_GRAPHICS);

		renderer->presentToSwapchain(mainWindow.get(), {});
	}
}

void KalosEngine::changeState(GameState *state)
{
	if (!gameStates.empty())
	{
		gameStates.back()->popped();
		gameStates.pop_back();
	}

	gameStates.push_back(state);
	gameStates.back()->pushed();
}

void KalosEngine::pushState(GameState *state)
{
	if (!gameStates.empty())
	{
		gameStates.back()->pause();
	}

	gameStates.push_back(state);
	gameStates.back()->pushed();
}

void KalosEngine::popState()
{
	if (!gameStates.empty())
	{
		gameStates.back()->popped();
		gameStates.pop_back();
	}

	if (!gameStates.empty())
	{
		gameStates.back()->resume();
	}
}

bool KalosEngine::isRunning()
{
	return engineIsRunning;
}

void KalosEngine::quit()
{
	engineIsRunning = false;
}

double KalosEngine::getTime()
{
	if (rendererBackendType == RENDERER_BACKEND_VULKAN || rendererBackendType == RENDERER_BACKEND_D3D12)
		return glfwGetTime();

	return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count() / 1000000000.0;
}

RendererBackend KalosEngine::getRendererBackendType()
{
	return rendererBackendType;
}

void KalosEngine::initColorTextures()
{
	whiteTexture2D = renderer->createTexture({2, 2, 1}, RESOURCE_FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT, MEMORY_USAGE_GPU_ONLY, false);
	whiteTexture2DView = renderer->createTextureView(whiteTexture2D);

	uint8_t whiteTexture2DData[16] = {
		255, 255, 255, 255,
		255, 255, 255, 255,
		255, 255, 255, 255,
		255, 255, 255, 255
	};

	StagingTexture whiteTexture2DStaging = renderer->createStagingTexture({2, 2, 1}, RESOURCE_FORMAT_R8G8B8A8_UNORM, 1, 1);
	renderer->fillStagingTextureSubresource(whiteTexture2DStaging, whiteTexture2DData, 0, 0);

	CommandBuffer fontAtlasUploadCommandBuffer = initCmdPool->allocateCommandBuffer();
	fontAtlasUploadCommandBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	ResourceBarrier barrier0 = {};
	barrier0.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
	barrier0.textureTransition.oldLayout = TEXTURE_LAYOUT_INITIAL_STATE;
	barrier0.textureTransition.newLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier0.textureTransition.subresourceRange = {0, 1, 0, 1};
	barrier0.textureTransition.texture = whiteTexture2D;

	ResourceBarrier barrier1 = {};
	barrier1.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
	barrier1.textureTransition.oldLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier1.textureTransition.newLayout = TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier1.textureTransition.subresourceRange = {0, 1, 0, 1};
	barrier1.textureTransition.texture = whiteTexture2D;

	fontAtlasUploadCommandBuffer->resourceBarriers({barrier0});
	fontAtlasUploadCommandBuffer->stageTextureSubresources(whiteTexture2DStaging, whiteTexture2D, {0, 1, 0, 1});
	fontAtlasUploadCommandBuffer->resourceBarriers({barrier1});

	fontAtlasUploadCommandBuffer->endCommands();

	Fence tempFence = renderer->createFence();
	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {fontAtlasUploadCommandBuffer}, {}, {}, {}, tempFence);

	renderer->waitForFence(tempFence, 5);

	initCmdPool->resetCommandPoolAndFreeCommandBuffer(fontAtlasUploadCommandBuffer);
	renderer->destroyFence(tempFence);
	renderer->destroyStagingTexture(whiteTexture2DStaging);
}

void KalosEngine::setupDebugInfoRenderGraph()
{
	nuklearCtx = new nk_context();
	nk_init_default(nuklearCtx, nullptr);

	DescriptorSetBinding guiSampler = {};
	guiSampler.binding = 0;
	guiSampler.arrayCount = 1;
	guiSampler.stageAccessMask = SHADER_STAGE_FRAGMENT_BIT;
	guiSampler.type = DESCRIPTOR_TYPE_SAMPLER;
	guiSampler.staticSamplers = {{}};

	DescriptorSetBinding guiTexture = {};
	guiTexture.binding = 1;
	guiTexture.arrayCount = 1;
	guiTexture.stageAccessMask = SHADER_STAGE_FRAGMENT_BIT;
	guiTexture.type = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;

	DescriptorSetLayoutDescription set0 = {};
	set0.bindings = {guiSampler, guiTexture};

	testFontAtlasDescriptrPool = renderer->createDescriptorPool(set0, 4);
	quadPassthroughDescriptorPool = renderer->createDescriptorPool(set0, 1);

	{
		nk_font_atlas atlas;

		nk_font_atlas_init_default(&atlas);
		nk_font_atlas_begin(&atlas);

		struct nk_font_config cfg = nk_font_config(0);
		cfg.oversample_h = 3;
		cfg.oversample_v = 2;

		std::vector<char> fontData = FileLoader::instance()->readFileBuffer("GameData/fonts/Cousine-Regular.ttf");
		font_clean = nk_font_atlas_add_from_memory(&atlas, fontData.data(), fontData.size(), 18, &cfg);

		int atlasWidth, atlasHeight;
		const void *imageData = nk_font_atlas_bake(&atlas, &atlasWidth, &atlasHeight, NK_FONT_ATLAS_RGBA32);

		testFontAtlas = renderer->createTexture({(uint32_t)atlasWidth, (uint32_t)atlasHeight, 1}, RESOURCE_FORMAT_R8G8B8A8_UNORM, TEXTURE_USAGE_TRANSFER_DST_BIT | TEXTURE_USAGE_SAMPLED_BIT, MEMORY_USAGE_GPU_ONLY, false);
		testFontAtlasView = renderer->createTextureView(testFontAtlas);

		StagingTexture stagingTexture = renderer->createStagingTexture({(uint32_t)atlasWidth, (uint32_t)atlasHeight, 1}, RESOURCE_FORMAT_R8G8B8A8_UNORM, 1, 1);
		renderer->fillStagingTextureSubresource(stagingTexture, imageData, 0, 0);

		CommandBuffer fontAtlasUploadCommandBuffer = initCmdPool->allocateCommandBuffer();
		fontAtlasUploadCommandBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

		ResourceBarrier barrier0 = {};
		barrier0.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
		barrier0.textureTransition.oldLayout = TEXTURE_LAYOUT_INITIAL_STATE;
		barrier0.textureTransition.newLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier0.textureTransition.subresourceRange = {0, 1, 0, 1};
		barrier0.textureTransition.texture = testFontAtlas;

		ResourceBarrier barrier1 = {};
		barrier1.barrierType = RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION;
		barrier1.textureTransition.oldLayout = TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier1.textureTransition.newLayout = TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier1.textureTransition.subresourceRange = {0, 1, 0, 1};
		barrier1.textureTransition.texture = testFontAtlas;

		fontAtlasUploadCommandBuffer->resourceBarriers({barrier0});
		fontAtlasUploadCommandBuffer->stageTextureSubresources(stagingTexture, testFontAtlas, {0, 1, 0, 1});
		fontAtlasUploadCommandBuffer->resourceBarriers({barrier1});

		fontAtlasUploadCommandBuffer->endCommands();

		Fence tempFence = renderer->createFence();
		renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {fontAtlasUploadCommandBuffer}, {}, {}, {}, tempFence);

		renderer->waitForFence(tempFence, 5);

		initCmdPool->resetCommandPoolAndFreeCommandBuffer(fontAtlasUploadCommandBuffer);
		renderer->destroyFence(tempFence);
		renderer->destroyStagingTexture(stagingTexture);

		defaultNKFontAtlasDescriptorSet = testFontAtlasDescriptrPool->allocateDescriptorSet();
		atlasNullDrawDescriptorSet = testFontAtlasDescriptrPool->allocateDescriptorSet();

		RendererDescriptorWriteInfo write0 = {};
		write0.dstBinding = 1;
		write0.dstArrayElement = 0;
		write0.descriptorType = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
		write0.textureInfo = {{testFontAtlasView, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
		renderer->writeDescriptorSets(defaultNKFontAtlasDescriptorSet, {write0});

		write0.dstBinding = 1;
		write0.dstArrayElement = 0;
		write0.descriptorType = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
		write0.textureInfo = {{get2DWhiteTextureView(), TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
		renderer->writeDescriptorSets(atlasNullDrawDescriptorSet, {write0});

		nk_font_atlas_end(&atlas, nk_handle_ptr(defaultNKFontAtlasDescriptorSet), nullptr);
		nk_style_set_font(nuklearCtx, &font_clean->handle);
	}

	{
		debugInfoRenderGraph = renderer->createRenderGraph();

		RenderPassAttachment debugInfoOut;
		debugInfoOut.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
		debugInfoOut.namedRelativeSize = "swapchain";

		RenderPassAttachment debugInfoOutDepth;
		debugInfoOutDepth.format = RESOURCE_FORMAT_D16_UNORM;
		debugInfoOutDepth.namedRelativeSize = "swapchain";

		auto &test = debugInfoRenderGraph->addRenderPass("test", RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS);
		test.addColorAttachmentOutput("debugInfoOut", debugInfoOut, true, {0.0f, 0.0f, 0.0f, 0.0f});
		test.setDepthStencilAttachmentOutput("debugInfoOutDepth", debugInfoOutDepth, true, {1, 0});

		test.setInitFunction(std::bind(&KalosEngine::debugInfoPassInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&KalosEngine::debugInfoPassRender, this, std::placeholders::_1, std::placeholders::_2));

		debugInfoRenderGraph->addNamedSize("swapchain", glm::uvec3(mainWindow->getWidth(), mainWindow->getHeight(), 1));
		debugInfoRenderGraph->setRenderGraphOutput("debugInfoOut");

		debugInfoRenderGraph->build();
	}

	debugInfoPassthroughDescriptorSet = quadPassthroughDescriptorPool->allocateDescriptorSet();
}

void KalosEngine::debugInfoPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data)
{
	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_GRAPHICS, debugInfoPassthroughPipeline);
	cmdBuffer->bindDescriptorSets(PIPELINE_BIND_POINT_GRAPHICS, 0, {debugInfoPassthroughDescriptorSet});
	cmdBuffer->draw(6, 1, 0, 0);

	nuklearRenderer->renderNuklearGUI(cmdBuffer, *nuklearCtx, mainWindow->getWidth(), mainWindow->getHeight());
}

void KalosEngine::debugInfoPassInit(const RenderGraphInitFunctionData &data)
{
	nuklearRenderer = std::unique_ptr<NuklearGUIRenderer>(new NuklearGUIRenderer(renderer.get(), data.renderPassHandle, atlasNullDrawDescriptorSet));

	ShaderModule vertShader = renderer->createShaderModule("GameData/shaders/quad-passthrough.hlsl", SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_HLSL, "QuadPasthroughVS");
	ShaderModule fragShader = renderer->createShaderModule("GameData/shaders/quad-passthrough.hlsl", SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_HLSL, "QuadPassthroughFS");

	PipelineShaderStage vertShaderStage = {};
	vertShaderStage.shaderModule = vertShader;

	PipelineShaderStage fragShaderStage = {};
	fragShaderStage.shaderModule = fragShader;

	PipelineVertexInputInfo vertexInput = {};
	vertexInput.vertexInputBindings = {};
	vertexInput.vertexInputAttribs = {};

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

	DescriptorStaticSampler staticSampler = {};
	staticSampler.magFilter = SAMPLER_FILTER_NEAREST;
	staticSampler.maxAnisotropy = 1.0f;

	DescriptorSetBinding passthroughSamplerBinding = {};
	passthroughSamplerBinding.binding = 0;
	passthroughSamplerBinding.arrayCount = 1;
	passthroughSamplerBinding.stageAccessMask = SHADER_STAGE_FRAGMENT_BIT;
	passthroughSamplerBinding.type = DESCRIPTOR_TYPE_SAMPLER;
	passthroughSamplerBinding.staticSamplers = {staticSampler};

	DescriptorSetBinding passthroughTextureBinding = {};
	passthroughTextureBinding.binding = 1;
	passthroughTextureBinding.arrayCount = 1;
	passthroughTextureBinding.stageAccessMask = SHADER_STAGE_FRAGMENT_BIT;
	passthroughTextureBinding.type = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;

	DescriptorSetLayoutDescription set0 = {};
	set0.bindings = {passthroughSamplerBinding, passthroughTextureBinding};

	GraphicsPipelineInfo info = {};
	info.stages = {vertShaderStage, fragShaderStage};
	info.vertexInputInfo = vertexInput;
	info.inputAssemblyInfo = inputAssembly;
	info.rasterizationInfo = rastInfo;
	info.depthStencilInfo = depthInfo;
	info.colorBlendInfo = colorBlend;

	info.inputPushConstants = {0, 0};
	info.inputSetLayouts = {set0};

	debugInfoPassthroughPipeline = renderer->createGraphicsPipeline(info, data.renderPassHandle, data.baseSubpass);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);
}

TextureView KalosEngine::get2DWhiteTextureView()
{
	return whiteTexture2DView;
}

struct nk_font *KalosEngine::getDefaultNKFont()
{
	return font_clean;
}

GameState::~GameState()
{

}