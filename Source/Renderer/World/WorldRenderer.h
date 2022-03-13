#ifndef RENDERER_WORLD_WORLDRENDERER_H_
#define RENDERER_WORLD_WORLDRENDERER_H_

#include <common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class KalosEngine;
class Renderer;

struct WorldRendererMaterialPushConstants
{
	uint32_t materialTextureIndex;
};

class WorldRenderer
{
public:

	WorldRenderer(KalosEngine *enginePtr);
	virtual ~WorldRenderer();

	void update(float delta);

	void gbufferPassInit(const RenderGraphInitFunctionData &data);
	void gbufferPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);

private:
	KalosEngine *engine;
	Renderer *renderer;

	Pipeline testMaterialPipeline;
};

#endif /* RENDERER_WORLD_WORLDRENDERER_H_ */
