#ifndef RENDERERCORE_TESTS_PUSHCONSTANTSEST_H_
#define RENDERERCORE_TESTS_PUSHCONSTANTSEST_H_

#include <common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class Renderer;

class PushConstantsTest
{
	public:

	Sampler renderTargetSampler;
	RenderGraph gfxGraph;

	Semaphore renderDoneSemaphore;

	PushConstantsTest(Renderer *rendererPtr);
	virtual ~PushConstantsTest();

	void render();

	private:

	Renderer *renderer;

	Pipeline gfxPipeline;

	float rotateCounter;

	void createPipeline(const RenderGraphInitFunctionData &data);

	void passInit(const RenderGraphInitFunctionData &data);
	void passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
};

#endif /* RENDERERCORE_TESTS_TRIANGLETEST_H_ */