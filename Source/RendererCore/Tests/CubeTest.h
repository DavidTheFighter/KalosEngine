#ifndef RENDERERCORE_TESTS_CUBETEST_H_
#define RENDERERCORE_TESTS_CUBETEST_H_

#include <common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class CubeTest
{
	public:

	Sampler renderTargetSampler;
	RenderGraph gfxGraph;

	Semaphore renderDoneSemaphore;

	CubeTest(Renderer *rendererPtr);
	virtual ~CubeTest();

	void render();

	private:

	Renderer *renderer;

	Pipeline gfxPipeline;

	Buffer cubeBuffer0;
	Buffer cubeBuffer1;
	Buffer cubeIndexBuffer;

	CommandPool cmdPool;

	float rotateCounter;

	void createPipeline(const RenderGraphInitFunctionData &data);

	void createBuffers();

	void passInit(const RenderGraphInitFunctionData &data);
	void passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
};

#endif /* RENDERERCORE_TESTS_CUBETEST_H_ */