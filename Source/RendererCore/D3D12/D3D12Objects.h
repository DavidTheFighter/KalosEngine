#ifndef RENDERING_D3D12_D3D12OBJECTS_H_
#define RENDERING_D3D12_D3D12OBJECTS_H_

#include <RendererCore/RendererObjects.h>

struct D3D12Texture : public RendererTexture
{
	ID3D12Resource *textureResource;
};

struct D3D12TextureView : public RendererTextureView
{
	ID3D12Resource *parentTextureResource;
};

struct D3D12Sampler : public RendererSampler
{
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
	GraphicsPipelineInfo gfxPipelineInfo;

	RenderPass renderPass;
};

struct D3D12DescriptorSet : public RendererDescriptorSet
{
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

struct D3D12Buffer : public RendererBuffer
{
	ID3D12Resource *bufferResource;
};

struct D3D12Fence : public RendererFence
{
	ID3D12Fence1 *fence;
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

#endif /* RENDERING_D3D12_D3D12OBJECTS_H_*/