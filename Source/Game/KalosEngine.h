
#ifndef GAME_KALOSENGINE_H_
#define GAME_KALOSENGINE_H_

#include <common.h>

#include <Game/GameState.h>

#include <Peripherals/Window.h>

#include <RendererCore/RendererCore.h>

class KalosEngine
{
	public:

	std::unique_ptr<RendererCore> renderer;
	std::unique_ptr<Window> mainWindow;

	KalosEngine(const std::vector<std::string> &launchArgs, RendererBackend rendererBackendType, uint32_t engineUpdateFrequencyCap = 250);
	virtual ~KalosEngine();

	void setEngineMaxUpdateFrequency(uint32_t engineUpdateFrequencyCap);

	void handleEvents();
	void update();
	void render();

	void changeState(GameState *state);
	void pushState(GameState *state);
	void popState();

	bool isRunning();
	void quit();

	double getTime();

	private:

	RendererBackend rendererBackendType;

	std::vector<std::string> launchArgs;
	std::vector<GameState*> gameStates;

	double lastUpdateTime;
	float lastLoopCPUTime;

	bool engineIsRunning;

	// Defines an upper limit to the frequency at which the game is allowed to update. Can be pretty high without causing any trouble. Defined in Hertz
	uint32_t updateFrequencyCap;
};

#endif /* GAME_KALOSENGINE_H_ */