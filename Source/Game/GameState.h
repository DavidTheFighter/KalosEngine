
#ifndef ENGINE_GAMESTATE_H_
#define ENGINE_GAMESTATE_H_

#include <common.h>

class KalosEngine;
class Window;

class GameState
{
	public:

		virtual ~GameState();

		virtual void pushed () = 0;
		virtual void popped () = 0;

		virtual void pause () = 0;
		virtual void resume () = 0;

		virtual void handleEvents () = 0;
		virtual void update (float delta) = 0;
		virtual void render () = 0;

		virtual struct RendererTextureView *getOutputTexture() = 0;
		virtual void windowResizeEvent(Window *window, uint32_t width, uint32_t height) = 0;
};

#endif /* ENGINE_GAMESTATE_H_ */
