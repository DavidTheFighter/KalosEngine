#ifndef RENDERERCORE_D3D12_D3D12RENDERGRAPH_H_
#define RENDERERCORE_D3D12_D3D12RENDERGRAPH_H_

#include <RendererCore/D3D12/d3d12_common.h>
#if BUILD_D3D12_BACKEND

#include <RendererCore/RendererRenderGraph.h>

#include <RendererCore/D3D12/D3D12Enums.h>
#include <RendererCore/D3D12/D3D12Objects.h>

typedef struct
{
	RenderPassAttachment attachment;
	D3D12Texture *rendererTexture;

	std::string name;

} D3D12RenderGraphTexture;

typedef struct
{
	size_t passIndex;

	std::vector<D3D12_RESOURCE_BARRIER> beforeRenderBarriers;
	std::vector<D3D12_RESOURCE_BARRIER> afterRenderBarriers;

	uint32_t sizeX;
	uint32_t sizeY;

} D3D12RenderGraphRenderPassData;

class D3D12Renderer;

class D3D12RenderGraph : public RendererRenderGraph
{
public:
	D3D12RenderGraph(D3D12Renderer *rendererPtr);
	virtual ~D3D12RenderGraph();

	Semaphore execute(bool returnWaitableSemaphore);

	TextureView getRenderGraphOutputTextureView();

	void resizeNamedSize(const std::string &sizeName, glm::uvec2 newSize);

private:

	D3D12Renderer *renderer;

	std::vector<D3D12RenderGraphRenderPassData> finalPasses;
	std::vector<size_t> finalPassStack;
	std::vector<Semaphore> executionDoneSemaphores;

	std::vector<CommandPool> gfxCommandPools;
	std::vector<CommandBuffer> gfxCommandBuffers;

	void cleanupResources();

	void assignPhysicalResources(const std::vector<size_t> &passStack);
	void finishBuild(const std::vector<size_t> &passStack);

	void buildBarrierLists(const std::vector<size_t> &passStack);

	std::vector<D3D12RenderGraphTexture> graphTextures;

	std::map<std::string, TextureView> graphTextureViews;
	std::map<std::string, Buffer> graphBuffers;
	std::map<std::string, D3D12_RESOURCE_STATES> graphResourcesInitialResourceState;
	std::map<size_t, ID3D12DescriptorHeap*> graphRTVs;
	std::map<size_t, ID3D12DescriptorHeap*> graphDSVs;

	size_t execCounter; // A counter incremented every time the execute() method is called, used for n-buffering of the command buffers

	UINT rtvDescriptorSize;
};

#endif
#endif /* RENDERERCORE_D3D12_D3D12RENDERGRAPH_H_*/