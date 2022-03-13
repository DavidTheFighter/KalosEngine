
#ifndef GAME_KALOSENGINE_H_
#define GAME_KALOSENGINE_H_

#include <common.h>

#include <Game/GameState.h>

#include <Peripherals/Window.h>

#include <RendererCore/Renderer.h>
#include <RendererCore/Tests/RenderTestHandler.h>

class NuklearGUIRenderer;
class WorldManager;
class ResourceManager;

class KalosEngine
{
	public:

	std::unique_ptr<Renderer> renderer;
	std::unique_ptr<Window> mainWindow;
	std::unique_ptr<WorldManager> worldManager;
	std::unique_ptr<ResourceManager> resourceManager;

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

	void enterNuklearInput(Window *window, struct nk_context *ctx);

	double getTime();

	RendererBackend getRendererBackendType();

	TextureView get2DWhiteTextureView();

	struct nk_font *getDefaultNKFont();

	private:

	RendererBackend rendererBackendType;

	std::vector<std::string> launchArgs;
	std::vector<GameState*> gameStates;

	struct nk_context *nuklearCtx;
	struct nk_font *font_clean;

	float updateFunctionTimer;
	float updatePacedDelta;

	glm::uvec2 mainWindowSize;

	double lastUpdateTime;
	float lastLoopCPUTime;

	bool engineIsRunning;

	TextureView currentGameStateOutput;
	TextureView currentEngineTextureOutput;
	Sampler swapchainSampler;

	CommandPool initCmdPool;

	Texture whiteTexture2D;
	TextureView whiteTexture2DView;

	DescriptorPool testFontAtlasDescriptrPool;
	DescriptorPool quadPassthroughDescriptorPool;

	Texture testFontAtlas;
	TextureView testFontAtlasView;

	DescriptorSet defaultNKFontAtlasDescriptorSet;
	DescriptorSet atlasNullDrawDescriptorSet;
	DescriptorSet debugInfoPassthroughDescriptorSet;

	RenderGraph debugInfoRenderGraph;

	Pipeline debugInfoPassthroughPipeline;

	// Defines an upper limit to the frequency at which the game is allowed to update. Can be pretty high without causing any trouble. Defined in Hertz
	uint32_t updateFrequencyCap;

	bool doingRenderingTest;
	std::unique_ptr<RenderTestHandler> renderTestHandler;
	std::unique_ptr<NuklearGUIRenderer> nuklearRenderer;

	void initColorTextures();
	void setupDebugInfoRenderGraph();

	void windowResizeCallback(Window *window, uint32_t width, uint32_t height);

	void debugInfoPassInit(const RenderGraphInitFunctionData &data);
	void debugInfoPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
};

#endif /* GAME_KALOSENGINE_H_ */