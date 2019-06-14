#ifndef GAME_GAMESTATETITLESCREEN_H_
#define GAME_GAMESTATETITLESCREEN_H_

#include "Game/GameState.h"
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class KalosEngine;
class NuklearGUIRenderer;

class GameStateTitleScreen : public GameState
{
public:
	GameStateTitleScreen(KalosEngine *enginePtr);
	~GameStateTitleScreen();

	void pushed();
	void popped();

	void pause();
	void resume();

	void handleEvents();
	void update(float delta);
	void render();

	TextureView getOutputTexture();

private:

	KalosEngine *engine;

	std::unique_ptr<NuklearGUIRenderer> nuklearRenderer;

	RenderGraph titleScreenRenderGraph;

	DescriptorPool testFontAtlasDescriptrPool;
	DescriptorSet atlasNullDrawDescriptorSet;

	struct nk_context *nuklearCtx;

	void titleScreenPassInit(const RenderGraphInitFunctionData& data);
	void titleScreenPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData& data);
};

#endif /* GAME_GAMESTATETITLESCREEN_H_ */