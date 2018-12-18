
#ifndef ENGINE_GAMESTATE_H_
#define ENGINE_GAMESTATE_H_

#include <common.h>

class KalosEngine;

class GameState
{
	public:

		virtual ~GameState();

		KalosEngine *engine;

		virtual void init () = 0;
		virtual void destroy () = 0;

		virtual void pause () = 0;
		virtual void resume () = 0;

		virtual void handleEvents () = 0;
		virtual void update (float delta) = 0;
		virtual void render () = 0;
};

#endif /* ENGINE_GAMESTATE_H_ */
