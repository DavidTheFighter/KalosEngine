#include "Game/GameStateInWorld.h"

#include <Game/KalosEngine.h>

#include <Resources/FileLoader.h>

#include <RendererCore/Renderer.h>

#include <Renderer/World/LightingRenderer.h>
#include <Renderer/World/WorldRenderer.h>

GameStateInWorld::GameStateInWorld(KalosEngine *enginePtr)
{
	engine = enginePtr;
	lightingRenderer = std::unique_ptr<LightingRenderer>(new LightingRenderer(enginePtr));
	worldRenderer = std::unique_ptr<WorldRenderer>(new WorldRenderer(enginePtr));

	inWorldRenderGraph = engine->renderer->createRenderGraph();

	RenderPassAttachment gbuffer0;
	gbuffer0.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
	gbuffer0.namedRelativeSize = "swapchain";

	RenderPassAttachment gbuffer1;
	gbuffer1.format = RESOURCE_FORMAT_R16G16_SFLOAT;
	gbuffer1.namedRelativeSize = "swapchain";

	RenderPassAttachment gbufferDepth;
	gbufferDepth.format = RESOURCE_FORMAT_D16_UNORM;
	gbufferDepth.namedRelativeSize = "swapchain";

	auto &gbufferPass = inWorldRenderGraph->addRenderPass("gbufferPass", RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS);
	gbufferPass.addColorAttachmentOutput("gbuffer0", gbuffer0, true, {0.0f, 0.0f, 0.0f, 0.0f});
	//gbufferPass.addColorAttachmentOutput("gbuffer1", gbuffer1, true, {0.0f, 0.0f, 0.0f, 0.0f});
	gbufferPass.setDepthStencilAttachmentOutput("gbufferDepth", gbufferDepth, true, {1, 0});

	gbufferPass.setInitFunction(std::bind(&WorldRenderer::gbufferPassInit, worldRenderer.get(), std::placeholders::_1));
	gbufferPass.setRenderFunction(std::bind(&WorldRenderer::gbufferPassRender, worldRenderer.get(), std::placeholders::_1, std::placeholders::_2));

	RenderPassAttachment lightingOutput;
	lightingOutput.format = RESOURCE_FORMAT_R16G16B16A16_SFLOAT;
	lightingOutput.namedRelativeSize = "swapchain";

	auto &lightingPass = inWorldRenderGraph->addRenderPass("lighting", RENDER_GRAPH_PIPELINE_TYPE_COMPUTE);
	lightingPass.addSampledTextureInput("gbuffer0");
	lightingPass.addSampledTextureInput("gbufferDepth");
	lightingPass.addStorageTexture("lightingOutput", lightingOutput, false, true);

	lightingPass.setInitFunction(std::bind(&LightingRenderer::lightingPassInit, lightingRenderer.get(), std::placeholders::_1));
	lightingPass.setRenderFunction(std::bind(&LightingRenderer::lightingPassRender, lightingRenderer.get(), std::placeholders::_1, std::placeholders::_2));
	lightingPass.setDescriptorUpdateFunction(std::bind(&LightingRenderer::lightingPassDescriptorUpdate, lightingRenderer.get(), std::placeholders::_1));

	RenderPassAttachment tonemapOutput;
	tonemapOutput.format = RESOURCE_FORMAT_R8G8B8A8_UNORM;
	tonemapOutput.namedRelativeSize = "swapchain";

	auto &tonemapPass = inWorldRenderGraph->addRenderPass("tonemap", RENDER_GRAPH_PIPELINE_TYPE_COMPUTE);
	tonemapPass.addStorageTexture("lightingOutput", lightingOutput, true, false);
	tonemapPass.addStorageTexture("tonemapOutput", tonemapOutput, false, true);

	tonemapPass.setInitFunction(std::bind(&LightingRenderer::tonemapPassInit, lightingRenderer.get(), std::placeholders::_1));
	tonemapPass.setRenderFunction(std::bind(&LightingRenderer::tonemapPassRender, lightingRenderer.get(), std::placeholders::_1, std::placeholders::_2));
	tonemapPass.setDescriptorUpdateFunction(std::bind(&LightingRenderer::tonemapPassDescriptorUpdate, lightingRenderer.get(), std::placeholders::_1));

	inWorldRenderGraph->addNamedSize("swapchain", glm::uvec3(engine->mainWindow->getWidth(), engine->mainWindow->getHeight(), 1));
	inWorldRenderGraph->setRenderGraphOutput("tonemapOutput");

	inWorldRenderGraph->build();
}

GameStateInWorld::~GameStateInWorld()
{

}

void GameStateInWorld::pushed()
{

}

void GameStateInWorld::popped()
{
	engine->renderer->destroyRenderGraph(inWorldRenderGraph);
	lightingRenderer.reset();
	worldRenderer.reset();
}

void GameStateInWorld::pause()
{

}

void GameStateInWorld::resume()
{

}

void GameStateInWorld::handleEvents()
{

}

void GameStateInWorld::update(float delta)
{
	worldRenderer->update(delta);
}

void GameStateInWorld::render()
{
	inWorldRenderGraph->execute(false);
}

TextureView GameStateInWorld::getOutputTexture()
{
	return inWorldRenderGraph->getRenderGraphOutputTextureView();
}

void GameStateInWorld::windowResizeEvent(Window *window, uint32_t width, uint32_t height)
{
	if (window == engine->mainWindow.get())
		inWorldRenderGraph->resizeNamedSize("swapchain", glm::uvec2(width, height));
}
