#ifndef RENDERERCORE_TESTS_CUBETEST_H_
#define RENDERERCORE_TESTS_CUBETEST_H_

#include <common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class Renderer;

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

	Buffer cubeBuffer;
	Buffer cubeIndexBuffer;

	Buffer cubeCBuffer;
	Sampler cubeTexSampler;
	Texture cubeTestTexture;
	TextureView cubeTestTextureView;

	CommandPool cmdPool;

	DescriptorPool cubeDescPool;
	DescriptorSet descSet;

	float rotateCounter;

	void createPipeline(const RenderGraphInitFunctionData &data);

	void createBuffers();
	void createTextures();

	void passInit(const RenderGraphInitFunctionData &data);
	void passRender(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData &data);
};

#endif /* RENDERERCORE_TESTS_CUBETEST_H_ */