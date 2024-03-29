#ifndef RENDERERCORE_TESTS_TRIANGLETEST_H_
#define RENDERERCORE_TESTS_TRIANGLETEST_H_

#include <common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class Renderer;

class TriangleTest
{
	public:

	Sampler renderTargetSampler;
	RenderGraph gfxGraph;

	Semaphore renderDoneSemaphore;

	TriangleTest(Renderer *rendererPtr);
	virtual ~TriangleTest();

	void render();

	private:

	Renderer *renderer;

	Pipeline gfxPipeline;

	void createPipeline(const RenderGraphInitFunctionData &data);

	void passInit(const RenderGraphInitFunctionData &data);
	void passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
};

#endif /* RENDERERCORE_TESTS_TRIANGLETEST_H_ */