#include "RendererCore/D3D12/D3D12CommandBuffer.h"

D3D12CommandBuffer::D3D12CommandBuffer()
{

}

D3D12CommandBuffer::~D3D12CommandBuffer()
{

}

void D3D12CommandBuffer::beginCommands(CommandBufferUsageFlags flags)
{
}

void D3D12CommandBuffer::endCommands()
{
}

void D3D12CommandBuffer::resetCommands()
{
}

void D3D12CommandBuffer::beginRenderPass(RenderPass renderPass, Framebuffer framebuffer, const Scissor & renderArea, const std::vector<ClearValue>& clearValues, SubpassContents contents)
{
}

void D3D12CommandBuffer::endRenderPass()
{
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
}

void D3D12CommandBuffer::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t vertexOffset, uint32_t firstInstance)
{
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
}

void D3D12CommandBuffer::setScissors(uint32_t firstScissor, const std::vector<Scissor>& scissors)
{
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
