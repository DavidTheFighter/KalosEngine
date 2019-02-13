#ifndef RENDERING_D3D12_D3D12OBJECTS_H_
#define RENDERING_D3D12_D3D12OBJECTS_H_
#if BUILD_D3D12_BACKEND

#include <RendererCore/RendererObjects.h>

struct D3D12Texture : public RendererTexture
{
	ID3D12Resource *textureResource;
};

struct D3D12TextureView : public RendererTextureView
{
};

struct D3D12Sampler : public RendererSampler
{
	D3D12_SAMPLER_DESC samplerDesc;
};

struct D3D12RenderPass : public RendererRenderPass
{
};

struct D3D12Framebuffer : public RendererFramebuffer
{
};

struct D3D12Pipeline : public RendererPipeline
{
	ID3D12PipelineState *pipeline;
	ID3D12RootSignature *rootSignature;

	RenderPass renderPass;
};

struct D3D12DescriptorSet : public RendererDescriptorSet
{
	ID3D12DescriptorHeap *srvUavCbvHeap;
	ID3D12DescriptorHeap *samplerHeap;

	uint32_t srvUavCbvMassHeapIndex;
	uint32_t samplerMassHeapIndex;

	uint32_t srvUavCbvStartDescriptorSlot;
	uint32_t samplerStartDescriptorSlot;

	uint32_t srvUavCbvDescriptorCount;
	uint32_t samplerDescriptorCount;

	uint32_t samplerCount;
	uint32_t constantBufferCount;
	uint32_t inputAtttachmentCount;
	uint32_t sampledTextureCount;

	DescriptorPool parentPool;
};

struct D3D12ShaderModule : public RendererShaderModule
{
	std::unique_ptr<char> shaderBytecode;
	size_t shaderBytecodeLength;
};

struct D3D12StagingBuffer : public RendererStagingBuffer
{
	ID3D12Resource *bufferResource;
};

struct D3D12StagingTexture : public RendererStagingTexture
{
	ID3D12Resource *bufferResource;
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> placedSubresourceFootprints;
	std::vector<uint32_t> subresourceNumRows;
	std::vector<uint64_t> subresourceRowSize;
};

struct D3D12Buffer : public RendererBuffer
{
	ID3D12Resource *bufferResource;
};

struct D3D12Fence : public RendererFence
{
	ID3D12Fence *fence;
	UINT64 fenceValue;
	HANDLE fenceEvent;

	std::atomic<bool> unsignaled; // Can be unsignaled, pending, or signaled
};

struct D3D12Semaphore : public RendererSemaphore
{
	ID3D12Fence *semFence;
	UINT64 semFenceValue;
	HANDLE semFenceWaitEvent;

	std::atomic<bool> pendingWait;
};

#include <RendererCore/D3D12/D3D12CommandBuffer.h>
#include <RendererCore/D3D12/D3D12CommandPool.h>
#include <RendererCore/D3D12/D3D12DescriptorPool.h>

#endif
#endif /* RENDERING_D3D12_D3D12OBJECTS_H_*/