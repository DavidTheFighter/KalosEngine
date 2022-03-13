#ifndef RENDERERCORE_TESTS_MSAATEST_H_
#define RENDERERCORE_TESTS_MSAATEST_H_

#include <common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class Renderer;

class MSAATest
{
	public:

	Sampler renderTargetSampler;
	RenderGraph gfxGraph;

	Semaphore renderDoneSemaphore;

	MSAATest(Renderer *rendererPtr);
	virtual ~MSAATest();

	void render();

	private:

	Renderer *renderer;

	Pipeline gfxPipeline;

	Texture msaaTexture;
	Texture resolvedTexture;

	void createPipeline(const RenderGraphInitFunctionData &data);

	void passInit(const RenderGraphInitFunctionData &data);
	void passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);

	void resolvePassDescriptorUpdate(const RenderGraphDescriptorUpdateFunctionData &data);
	void resolvePassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
};

#endif /* RENDERERCORE_TESTS_MSAATEST_H_ */