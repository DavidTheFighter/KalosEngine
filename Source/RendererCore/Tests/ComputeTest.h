#ifndef RENDERERCORE_TESTS_COMPUTETEST_H_
#define RENDERERCORE_TESTS_COMPUTETEST_H_

#include <common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class Renderer;

class ComputeTest
{
	public:

	Sampler renderTargetSampler;
	RenderGraph gfxGraph;

	Semaphore renderDoneSemaphore;

	ComputeTest(Renderer *rendererPtr);
	virtual ~ComputeTest();

	void render();

	private:

	Renderer *renderer;

	Pipeline bufferGenPipeline;
	Pipeline compPipeline;

	CommandPool cmdPool;
	DescriptorPool descPool;
	DescriptorSet descSet;

	Buffer testStorageBuffer;

	void createBufferGenPipeline(const RenderGraphInitFunctionData &data);
	void createPipeline(const RenderGraphInitFunctionData &data);

	void bufferGenPassRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);

	void passInit(const RenderGraphInitFunctionData &data);
	void passDescUpdate(const RenderGraphDescriptorUpdateFunctionData& data);
	void passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
};

#endif /* RENDERERCORE_TESTS_COMPUTETEST_H_ */
