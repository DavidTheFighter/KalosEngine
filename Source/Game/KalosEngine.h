
#ifndef GAME_KALOSENGINE_H_
#define GAME_KALOSENGINE_H_

#include <common.h>

#include <Game/GameState.h>

#include <Peripherals/Window.h>

#include <RendererCore/Renderer.h>

class TriangleTest;
class VertexIndexBufferTest;
class CubeTest;

typedef enum
{
	RENDERER_TEST_TRIANGLE, // Tests the bare basics, as little as possible to get something rendered to the screen
	RENDERER_TEST_VERTEX_INDEX_BUFFER, // Tests vertex and index buffers by drawing two triangles
	RENDERER_TEST_PUSH_CONSTANTS, // Tests push constants by making a 3D rotating triangle
	RENDERER_TEST_CUBE, // Combines vertex/index buffer, push constants, depth buffer + more to make a rotating cube like the vkcube.exe demo
	RENDERER_TEST_DESCRIPTORS // Tests resource binding of static samplers, dynamic samplers, constant buffers, and textures
} RendererTest;

class KalosEngine
{
	public:

	std::unique_ptr<Renderer> renderer;
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

	glm::uvec2 mainWindowSize;

	double lastUpdateTime;
	float lastLoopCPUTime;

	bool engineIsRunning;

	// Defines an upper limit to the frequency at which the game is allowed to update. Can be pretty high without causing any trouble. Defined in Hertz
	uint32_t updateFrequencyCap;

	bool doingRenderingTest;
	RendererTest currentRenderingTest;

	std::unique_ptr<TriangleTest> triangleTest;
	std::unique_ptr<VertexIndexBufferTest> vertexIndexBufferTest;
	std::unique_ptr<CubeTest> cubeTest;
};

#endif /* GAME_KALOSENGINE_H_ */