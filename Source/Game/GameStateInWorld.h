#ifndef GAME_GAMESTATEINWORLD_H_
#define GAME_GAMESTATEINWORLD_H_

#include "Game/GameState.h"
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class KalosEngine;
class LightingRenderer;
class WorldRenderer;

class GameStateInWorld : public GameState
{
public:
	GameStateInWorld(KalosEngine *enginePtr);
	virtual ~GameStateInWorld();

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

	std::unique_ptr<LightingRenderer> lightingRenderer;
	std::unique_ptr<WorldRenderer> worldRenderer;

	RenderGraph inWorldRenderGraph;
};

#endif /* GAME_GAMESTATEINWORLD_H_ */

