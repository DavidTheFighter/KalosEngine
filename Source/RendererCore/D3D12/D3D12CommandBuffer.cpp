#include "RendererCore/D3D12/D3D12CommandBuffer.h"

#include <RendererCore/D3D12/D3D12CommandPool.h>

D3D12CommandBuffer::D3D12CommandBuffer(D3D12CommandPool *parentCommandPoolPtr, D3D12_COMMAND_LIST_TYPE commandListType)
{
	parentCmdPool = parentCommandPoolPtr;
	cmdListType = commandListType;

	cmdList = nullptr;

	ctx_inRenderPass = false;
}

D3D12CommandBuffer::~D3D12CommandBuffer()
{

}

void D3D12CommandBuffer::beginCommands(CommandBufferUsageFlags flags)
{
	DX_CHECK_RESULT(parentCmdPool->device->CreateCommandList(0, cmdListType, parentCmdPool->cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList)));
}

void D3D12CommandBuffer::endCommands()
{
	DX_CHECK_RESULT(cmdList->Close());
}

void D3D12CommandBuffer::resetCommands()
{
	
}

void D3D12CommandBuffer::beginRenderPass(RenderPass renderPass, Framebuffer framebuffer, const Scissor & renderArea, const std::vector<ClearValue>& clearValues, SubpassContents contents)
{
	if (ctx_inRenderPass)
	{
		Log::get()->error("D3D12CommandBuffer: Can't begin a new render pass in command buffer {}, it's still in a render pass, call D3D12CommandBuffer::endRenderPass() first before begining another", (void*) this);

		return;
	}

	ctx_inRenderPass = true;
	ctx_currentRenderPass = renderPass;

	const SubpassDescription &subpass0 = renderPass->subpasses[0];

	std::vector<D3D12_RESOURCE_BARRIER> subpass0Barriers;

}

void D3D12CommandBuffer::endRenderPass()
{
	if (!ctx_inRenderPass)
	{
		Log::get()->error("D3D12CommandBuffer: Can't end a render pass in command buffer {}, it's not currently in one. Call D3D12CommandBuffer::beginRenderPass() first before ending one", (void*) this);

		return;
	}

	ctx_inRenderPass = false;
}

void D3D12CommandBuffer::nextSubpass(SubpassContents contents)
{
}

void D3D12CommandBuffer::bindPipeline(PipelineBindPoint point, Pipeline pipeline)
{
}

void D3D12CommandBuffer::bindIndexBuffer(Buffer buffer, size_t offset, bool uses32BitIndices)
{
}

void D3D12CommandBuffer::bindVertexBuffers(uint32_t firstBinding, const std::vector<Buffer>& buffers, const std::vector<size_t>& offsets)
{
}

void D3D12CommandBuffer::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	cmdList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
}

void D3D12CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t firstVertex, uint32_t firstInstance)
{
	cmdList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void D3D12CommandBuffer::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
}

void D3D12CommandBuffer::pushConstants(ShaderStageFlags stages, uint32_t offset, uint32_t size, const void * data)
{
}

void D3D12CommandBuffer::bindDescriptorSets(PipelineBindPoint point, uint32_t firstSet, std::vector<DescriptorSet> sets)
{
}

void D3D12CommandBuffer::transitionTextureLayout(Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource)
{
}

void D3D12CommandBuffer::setTextureLayout(Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource, PipelineStageFlags srcStage, PipelineStageFlags dstStage)
{
}

void D3D12CommandBuffer::stageBuffer(StagingBuffer stagingBuffer, Texture dstTexture, TextureSubresourceLayers subresource, sivec3 offset, suvec3 extent)
{
}

void D3D12CommandBuffer::stageBuffer(StagingBuffer stagingBuffer, Buffer dstBuffer)
{
}

void D3D12CommandBuffer::setViewports(uint32_t firstViewport, const std::vector<Viewport>& viewports)
{
	cmdList->RSSetViewports((UINT) viewports.size(), reinterpret_cast<const D3D12_VIEWPORT*>(viewports.data()));
}

void D3D12CommandBuffer::setScissors(uint32_t firstScissor, const std::vector<Scissor>& scissors)
{
	std::vector<D3D12_RECT> d3d12ScissorRects(scissors.size());

	for (const Scissor &s : scissors)
		d3d12ScissorRects.push_back({(LONG) s.x, (LONG) s.y, (LONG) s.width, (LONG) s.height});

	cmdList->RSSetScissorRects((UINT) scissors.size(), d3d12ScissorRects.data());

}

void D3D12CommandBuffer::blitTexture(Texture src, TextureLayout srcLayout, Texture dst, TextureLayout dstLayout, std::vector<TextureBlitInfo> blitRegions, SamplerFilter filter)
{
}

void D3D12CommandBuffer::beginDebugRegion(const std::string & regionName, glm::vec4 color)
{
}

void D3D12CommandBuffer::endDebugRegion()
{
}

void D3D12CommandBuffer::insertDebugMarker(const std::string & markerName, glm::vec4 color)
{
}
