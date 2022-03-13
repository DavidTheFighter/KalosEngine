#ifndef RENDERER_WORLD_LIGHTINGRENDERER_H_
#define RENDERER_WORLD_LIGHTINGRENDERER_H_

#include <common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class KalosEngine;
class Renderer;

struct TonemapPushConstantsBuffer 
{
	float exposure;
	float invGamma;
};

class LightingRenderer
{
public:
	LightingRenderer(KalosEngine *enginePtr);
	virtual ~LightingRenderer();

	void lightingPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
	void lightingPassInit(const RenderGraphInitFunctionData &data);
	void lightingPassDescriptorUpdate(const RenderGraphDescriptorUpdateFunctionData &data);

	void tonemapPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
	void tonemapPassInit(const RenderGraphInitFunctionData &data);
	void tonemapPassDescriptorUpdate(const RenderGraphDescriptorUpdateFunctionData &data);


private:
	KalosEngine *engine;
	Renderer *renderer;

	glm::uvec2 swapchainSize;

	Pipeline lightingPipeline;
	Pipeline tonemapPipeline;

	DescriptorPool lightingDescriptorPool;
	DescriptorSet lightingDescriptorSet;

	DescriptorPool tonemapDescriptorPool;
	DescriptorSet tonemapDescriptorSet;
};

#endif /* RENDERER_WORLD_LIGHTINGRENDERER_H_ */