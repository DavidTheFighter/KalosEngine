#ifndef RENDERERCORE_D3D12_D3D12RENDERGRAPH_H_
#define RENDERERCORE_D3D12_D3D12RENDERGRAPH_H_

#include <RendererCore/RendererRenderGraph.h>

#include <RendererCore/D3D12/d3d12_common.h>
#include <RendererCore/D3D12/D3D12Enums.h>
#include <RendererCore/D3D12/D3D12Objects.h>

typedef struct
{
	RenderPassAttachment attachment;
	ID3D12Resource *textureHeap;

} D3D12RenderGraphTexture;

class D3D12Renderer;

class D3D12RenderGraph : public RendererRenderGraph
{
public:
	D3D12RenderGraph(D3D12Renderer *rendererPtr);
	virtual ~D3D12RenderGraph();

	Semaphore execute();

	TextureView getRenderGraphOutputTextureView();

private:

	D3D12Renderer *d3drenderer;

	std::vector<size_t> finalPassStack;
	std::vector<Semaphore> executionDoneSemaphores;

	std::vector<CommandPool> gfxCommandPools;
	std::vector<CommandBuffer> gfxCommandBuffers;

	void cleanupResources();

	void resizeNamedSize(const std::string &sizeName, glm::uvec2 newSize);
	void assignPhysicalResources(const std::vector<size_t> &passStack);
	void finishBuild(const std::vector<size_t> &passStack);

	std::vector<D3D12RenderGraphTexture> graphTextures;

	std::map<std::string, RenderPassOutputAttachment> allAttachments;
	std::map<std::string, TextureView> graphTextureViews;
	std::map<size_t, ID3D12DescriptorHeap*> graphRTVs;
	std::map<size_t, ID3D12DescriptorHeap*> graphDSVs;

	size_t execCounter; // A counter incremented every time the execute() method is called, used for n-buffering of the command buffers

	UINT rtvDescriptorSize;
};

#endif /* RENDERERCORE_D3D12_D3D12RENDERGRAPH_H_*/