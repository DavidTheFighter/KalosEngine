#ifndef RENDERERCORE_TESTS_VERTEXINDEXBUFFERTEST_H_
#define RENDERERCORE_TESTS_VERTEXINDEXBUFFERTEST_H_

#include <common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class Renderer;

class VertexIndexBufferTest
{
	public:

	Sampler renderTargetSampler;
	RenderGraph gfxGraph;

	Semaphore renderDoneSemaphore;

	VertexIndexBufferTest(Renderer *rendererPtr);
	virtual ~VertexIndexBufferTest();

	void render();

	private:

	Renderer *renderer;

	Pipeline gfxPipeline;
	CommandPool cmdPool;

	Buffer positionBuffers[2];
	Buffer colorBuffer;
	Buffer indexBuffer;

	void createPipeline(const RenderGraphInitFunctionData &data);
	void createBuffers();

	void passInit(const RenderGraphInitFunctionData &data);
	void passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
};

#endif /* RENDERERCORE_TESTS_TRIANGLETEST_H_ */