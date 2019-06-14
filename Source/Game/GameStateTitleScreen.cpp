#include "Game/GameStateTitleScreen.h"

#include <Game/KalosEngine.h>

#include <Resources/FileLoader.h>

#include <RendererCore/Renderer.h>

#include <Renderer/NuklearGUIRenderer.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#include <nuklear.h>

GameStateTitleScreen::GameStateTitleScreen(KalosEngine *enginePtr)
{
	engine = enginePtr;

	nuklearCtx = new nk_context();
	nk_init_default(nuklearCtx, nullptr);
	nk_style_set_font(nuklearCtx, &engine->getDefaultNKFont()->handle);

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

	testFontAtlasDescriptrPool = engine->renderer->createDescriptorPool(set0, 1);

	CommandPool testCmdPool = engine->renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, 0);

	{
		atlasNullDrawDescriptorSet = testFontAtlasDescriptrPool->allocateDescriptorSet();

		RendererDescriptorWriteInfo write0 = {};
		write0.dstBinding = 1;
		write0.dstArrayElement = 0;
		write0.descriptorType = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;
		write0.textureInfo = {{engine->get2DWhiteTextureView(), TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
		engine->renderer->writeDescriptorSets(atlasNullDrawDescriptorSet, {write0});
	}

	{
		titleScreenRenderGraph = engine->renderer->createRenderGraph();

		RenderPassAttachment titleScreenOut;
		titleScreenOut.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
		titleScreenOut.namedRelativeSize = "swapchain";
		
		RenderPassAttachment titleScreenOutDepth;
		titleScreenOutDepth.format = RESOURCE_FORMAT_D16_UNORM;
		titleScreenOutDepth.namedRelativeSize = "swapchain";

		auto& test = titleScreenRenderGraph->addRenderPass("test", RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS);
		test.addColorAttachmentOutput("titleScreenOut", titleScreenOut, true, {0.0f, 0.0f, 0.0f, 0.0f});
		test.setDepthStencilAttachmentOutput("titleScreenOutDepth", titleScreenOutDepth, true, {1, 0});

		test.setInitFunction(std::bind(&GameStateTitleScreen::titleScreenPassInit, this, std::placeholders::_1));
		test.setRenderFunction(std::bind(&GameStateTitleScreen::titleScreenPassRender, this, std::placeholders::_1, std::placeholders::_2));

		titleScreenRenderGraph->addNamedSize("swapchain", glm::uvec3(engine->mainWindow->getWidth(), engine->mainWindow->getHeight(), 1));
		titleScreenRenderGraph->setRenderGraphOutput("titleScreenOut");

		titleScreenRenderGraph->build();
	}


	engine->renderer->destroyCommandPool(testCmdPool);
}

GameStateTitleScreen::~GameStateTitleScreen()
{
	nk_free(nuklearCtx);
	delete nuklearCtx;
}

void GameStateTitleScreen::pushed()
{

}

void GameStateTitleScreen::popped()
{
	engine->renderer->destroyDescriptorPool(testFontAtlasDescriptrPool);

	engine->renderer->destroyRenderGraph(titleScreenRenderGraph);

	nuklearRenderer.reset();
}

void GameStateTitleScreen::pause()
{

}

void GameStateTitleScreen::resume()
{

}

void GameStateTitleScreen::handleEvents()
{

}

void GameStateTitleScreen::update(float delta)
{
	nk_clear(nuklearCtx);

	if (nk_begin(nuklearCtx, "DebugText", nk_rect(0, 0, 1920, 1080), NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND | NK_WINDOW_NO_INPUT))
	{
		enum
		{
			EASY, HARD
		};
		static int op = EASY;
		static int property = 20;
		nk_layout_row_static(nuklearCtx, 30, 80, 1);
		if (nk_button_label(nuklearCtx, "button"))
			fprintf(stdout, "button pressed\n");

		nk_layout_row_dynamic(nuklearCtx, 30, 2);
		if (nk_option_label(nuklearCtx, "easy", op == EASY))
			op = EASY;
		if (nk_option_label(nuklearCtx, "hard", op == HARD))
			op = HARD;

		nk_layout_row_dynamic(nuklearCtx, 25, 1);
		nk_property_int(nuklearCtx, "Compression:", 0, &property, 100, 10, 1);
	}
	nk_end(nuklearCtx);
}

void GameStateTitleScreen::render()
{
	titleScreenRenderGraph->execute(false);
}

void GameStateTitleScreen::titleScreenPassInit(const RenderGraphInitFunctionData& data)
{
	nuklearRenderer = std::unique_ptr<NuklearGUIRenderer>(new NuklearGUIRenderer(engine->renderer.get(), data.renderPassHandle, atlasNullDrawDescriptorSet));
}

void GameStateTitleScreen::titleScreenPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData& data)
{
	nuklearRenderer->renderNuklearGUI(cmdBuffer, *nuklearCtx, engine->mainWindow->getWidth(), engine->mainWindow->getHeight());
}

TextureView GameStateTitleScreen::getOutputTexture()
{
	return titleScreenRenderGraph->getRenderGraphOutputTextureView();
}