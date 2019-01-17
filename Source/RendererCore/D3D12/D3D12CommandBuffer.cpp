#include "RendererCore/D3D12/D3D12CommandBuffer.h"

#include <RendererCore/D3D12/D3D12Enums.h>
#include <RendererCore/D3D12/D3D12Objects.h>


D3D12CommandBuffer::D3D12CommandBuffer(D3D12CommandPool *parentCommandPoolPtr, D3D12_COMMAND_LIST_TYPE commandListType)
{
	parentCmdPool = parentCommandPoolPtr;
	cmdListType = commandListType;

	cmdList = nullptr;

	startedRecording = false;
}

D3D12CommandBuffer::~D3D12CommandBuffer()
{
	if (cmdList != nullptr)
		cmdList->Release();
}

void D3D12CommandBuffer::beginCommands(CommandBufferUsageFlags flags)
{
	if (startedRecording)
	{
		Log::get()->error("D3D12CommandBuffer: Cannot record to a command buffer that has already been recorded too. Reset it first or allocate a new one");

		return;
	}

	if (cmdList == nullptr)
	{
		DX_CHECK_RESULT(parentCmdPool->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, parentCmdPool->cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList)));
	}
	else
	{
		cmdList->Reset(parentCmdPool->cmdAlloc, nullptr);
	}

	startedRecording = true;
}

void D3D12CommandBuffer::endCommands()
{
	DX_CHECK_RESULT(cmdList->Close());
}

void D3D12CommandBuffer::d3d12_reset()
{
	if (cmdList != nullptr)
		DX_CHECK_RESULT(cmdList->Reset(parentCmdPool->cmdAlloc, nullptr));
}

void D3D12CommandBuffer::bindPipeline(PipelineBindPoint point, Pipeline pipeline)
{
	D3D12Pipeline *d3dpipeline = static_cast<D3D12Pipeline*>(pipeline);

	cmdList->SetGraphicsRootSignature(d3dpipeline->rootSignature);
	cmdList->SetPipelineState(d3dpipeline->pipeline);

	cmdList->IASetPrimitiveTopology(primitiveTopologyToD3DPrimitiveTopology(d3dpipeline->gfxPipelineInfo.inputAssemblyInfo.topology, d3dpipeline->gfxPipelineInfo.tessellationPatchControlPoints));
	cmdList->OMSetBlendFactor(d3dpipeline->gfxPipelineInfo.colorBlendInfo.blendConstants);
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
	std::vector<D3D12_VIEWPORT> d3dViewports;

	for (const Viewport &vp : viewports)
	{
		D3D12_VIEWPORT d3dViewport = {};
		d3dViewport.TopLeftX = vp.x;
		d3dViewport.TopLeftY = vp.y;
		d3dViewport.Width = vp.width;
		d3dViewport.Height = vp.height;
		d3dViewport.MinDepth = vp.minDepth;
		d3dViewport.MaxDepth = vp.maxDepth;

		d3dViewports.push_back(d3dViewport);
	}

	cmdList->RSSetViewports((UINT)d3dViewports.size(), d3dViewports.data());
}

void D3D12CommandBuffer::setScissors(uint32_t firstScissor, const std::vector<Scissor>& scissors)
{
	std::vector<D3D12_RECT> d3d12ScissorRects;

	for (const Scissor &s : scissors)
	{
		D3D12_RECT rect = {};
		rect.left = s.x;
		rect.right = (LONG) s.width;
		rect.top = s.y;
		rect.bottom = (LONG) s.height;

		d3d12ScissorRects.push_back(rect);
	}

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

void D3D12CommandBuffer::d3d12_clearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, ClearValue clearValue)
{
	cmdList->ClearRenderTargetView(RenderTargetView, clearValue.color.float32, 0, nullptr);
}

void D3D12CommandBuffer::d3d12_clearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView, float depthClearValue, uint8_t stencilClearValue)
{
	cmdList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, depthClearValue, stencilClearValue, 0, nullptr);
}

void D3D12CommandBuffer::d3d12_OMSetRenderTargets(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> &rtvDescriptors, bool singleHandle, const D3D12_CPU_DESCRIPTOR_HANDLE *dsvDescriptor)
{
	cmdList->OMSetRenderTargets(rtvDescriptors.size(), rtvDescriptors.data(), singleHandle, dsvDescriptor);
}

void D3D12CommandBuffer::d3d12_resourceBarrier(const std::vector<D3D12_RESOURCE_BARRIER> &barriers)
{
	cmdList->ResourceBarrier(barriers.size(), barriers.data());
}