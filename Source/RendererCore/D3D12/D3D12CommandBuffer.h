#ifndef RENDERING_D3D12_D3D12COMMANDBUFFER_H_
#define RENDERING_D3D12_D3D12COMMANDBUFFER_H_

#include <RendererCore/D3D12/d3d12_common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class D3D12CommandPool;
class D3D12Pipeline;

class D3D12CommandBuffer : public RendererCommandBuffer
{
	public:

	D3D12CommandBuffer(D3D12CommandPool *parentCommandPoolPtr, D3D12_COMMAND_LIST_TYPE commandListType);
	virtual ~D3D12CommandBuffer();

	void beginCommands(CommandBufferUsageFlags flags);
	void endCommands();

	void bindPipeline(PipelineBindPoint point, Pipeline pipeline);

	void bindIndexBuffer(Buffer buffer, size_t offset, bool uses32BitIndices);
	void bindVertexBuffers(uint32_t firstBinding, const std::vector<Buffer> &buffers, const std::vector<size_t> &offsets);

	void draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
	void drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t firstVertex, uint32_t firstInstance);
	void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

	void pushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data);
	void bindDescriptorSets(PipelineBindPoint point, uint32_t firstSet, std::vector<DescriptorSet> sets);

	void transitionTextureLayout(Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource);
	void setTextureLayout(Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource, PipelineStageFlags srcStage, PipelineStageFlags dstStage);

	void stageBuffer(StagingBuffer stagingBuffer, Texture dstTexture, TextureSubresourceLayers subresource, sivec3 offset, suvec3 extent);
	void stageBuffer(StagingBuffer stagingBuffer, Buffer dstBuffer);

	void setViewports(uint32_t firstViewport, const std::vector<Viewport> &viewports);
	void setScissors(uint32_t firstScissor, const std::vector<Scissor> &scissors);

	void blitTexture(Texture src, TextureLayout srcLayout, Texture dst, TextureLayout dstLayout, std::vector<TextureBlitInfo> blitRegions, SamplerFilter filter);

	void beginDebugRegion(const std::string &regionName, glm::vec4 color);
	void endDebugRegion();
	void insertDebugMarker(const std::string &markerName, glm::vec4 color);

	void d3d12_clearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, ClearValue clearValue);
	void d3d12_clearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView, float depthClearValue, uint8_t stencilClearValue);
	void d3d12_OMSetRenderTargets(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> &rtvDescriptors, bool singleHandle, const D3D12_CPU_DESCRIPTOR_HANDLE *dsvDescriptor);
	void d3d12_resourceBarrier(const std::vector<D3D12_RESOURCE_BARRIER> &barriers);
	void d3d12_reset();

	private:

	D3D12CommandPool *parentCmdPool;
	D3D12_COMMAND_LIST_TYPE cmdListType;

	ID3D12GraphicsCommandList *cmdList;

	D3D12Pipeline *cxt_currentGraphicsPipeline;

	bool startedRecording;

	friend class D3D12CommandPool;
	friend class D3D12Renderer;
};

#endif /* RENDERING_D3D12_D3D12COMMANDBUFFER_H_ */