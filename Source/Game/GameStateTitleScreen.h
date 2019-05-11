#ifndef GAME_GAMESTATETITLESCREEN_H_
#define GAME_GAMESTATETITLESCREEN_H_

#include "Game/GameState.h"

class KalosEngine;

class GameStateTitleScreen : public GameState
{
public:
	GameStateTitleScreen(KalosEngine *enginePtr);
	~GameStateTitleScreen();

	void init();
	void destroy();

	void pause();
	void resume();

	void handleEvents();
	void update(float delta);
	void render();

private:

	KalosEngine *engine;
};

#endif /* GAME_GAMESTATETITLESCREEN_H_ */