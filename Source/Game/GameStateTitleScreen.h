#ifndef GAME_GAMESTATETITLESCREEN_H_
#define GAME_GAMESTATETITLESCREEN_H_

#include "Game/GameState.h"
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class KalosEngine;
class NuklearGUIRenderer;
class GameStateInWorld;

class GameStateTitleScreen : public GameState
{
public:
	GameStateTitleScreen(KalosEngine *enginePtr, GameStateInWorld *inWorldGameStatePtr);
	~GameStateTitleScreen();

	void pushed();
	void popped();

	void pause();
	void resume();

	void handleEvents();
	void update(float delta);
	void render();

	TextureView getOutputTexture();
	void windowResizeEvent(Window *window, uint32_t width, uint32_t height);

private:

	KalosEngine *engine;
	GameStateInWorld *inWorldGameState;

	std::unique_ptr<NuklearGUIRenderer> nuklearRenderer;

	RenderGraph titleScreenRenderGraph;

	DescriptorPool testFontAtlasDescriptrPool;
	DescriptorSet atlasNullDrawDescriptorSet;

	struct nk_context *nuklearCtx;

	void titleScreenPassInit(const RenderGraphInitFunctionData& data);
	void titleScreenPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData& data);
};

#endif /* GAME_GAMESTATETITLESCREEN_H_ */