#include "RendererCore/D3D12/D3D12CommandBuffer.h"
#if BUILD_D3D12_BACKEND

#include <RendererCore/D3D12/D3D12Renderer.h>
#include <RendererCore/D3D12/D3D12Enums.h>
#include <RendererCore/D3D12/D3D12Objects.h>

D3D12CommandBuffer::D3D12CommandBuffer(D3D12Renderer *rendererPtr, D3D12CommandPool *parentCommandPoolPtr, D3D12_COMMAND_LIST_TYPE commandListType)
{
	renderer = rendererPtr;
	parentCmdPool = parentCommandPoolPtr;
	cmdListType = commandListType;

	cmdList = nullptr;

	startedRecording = false;

	cxt_currentGraphicsPipeline = nullptr;
	ctx_currentBoundSamplerDescHeap = nullptr;
	ctx_currentBoundSrvUavCbvDescHeap = nullptr;
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
		DX_CHECK_RESULT(renderer->device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, parentCmdPool->cmdAlloc, nullptr, IID_PPV_ARGS(&cmdList)));
	}
	else
	{
		cmdList->Reset(parentCmdPool->cmdAlloc, nullptr);
	}

	startedRecording = true;
	cxt_currentGraphicsPipeline = nullptr;
	ctx_currentBoundSamplerDescHeap = nullptr;
	ctx_currentBoundSrvUavCbvDescHeap = nullptr;
}

void D3D12CommandBuffer::endCommands()
{
	DX_CHECK_RESULT(cmdList->Close());

	cxt_currentGraphicsPipeline = nullptr;
	ctx_currentBoundSamplerDescHeap = nullptr;
	ctx_currentBoundSrvUavCbvDescHeap = nullptr;
}

void D3D12CommandBuffer::d3d12_reset()
{
	if (cmdList != nullptr)
		DX_CHECK_RESULT(cmdList->Reset(parentCmdPool->cmdAlloc, nullptr));
}

void D3D12CommandBuffer::bindPipeline(PipelineBindPoint point, Pipeline pipeline)
{
	D3D12Pipeline *d3dpipeline = static_cast<D3D12Pipeline*>(pipeline);
	cxt_currentGraphicsPipeline = d3dpipeline;

	cmdList->SetGraphicsRootSignature(d3dpipeline->rootSignature);
	cmdList->SetPipelineState(d3dpipeline->pipeline);

	cmdList->IASetPrimitiveTopology(primitiveTopologyToD3DPrimitiveTopology(d3dpipeline->gfxPipelineInfo.inputAssemblyInfo.topology, d3dpipeline->gfxPipelineInfo.tessellationPatchControlPoints));
	cmdList->OMSetBlendFactor(d3dpipeline->gfxPipelineInfo.colorBlendInfo.blendConstants);
}

void D3D12CommandBuffer::bindIndexBuffer(Buffer buffer, size_t offset, bool uses32BitIndices)
{
	D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
	indexBufferView.BufferLocation = static_cast<D3D12Buffer*>(buffer)->bufferResource->GetGPUVirtualAddress() + offset;
	indexBufferView.Format = uses32BitIndices ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	indexBufferView.SizeInBytes = static_cast<D3D12Buffer*>(buffer)->bufferSize;

	cmdList->IASetIndexBuffer(&indexBufferView);
}

void D3D12CommandBuffer::bindVertexBuffers(uint32_t firstBinding, const std::vector<Buffer>& buffers, const std::vector<size_t>& offsets)
{
	DEBUG_ASSERT(buffers.size() == offsets.size());

	std::map<uint32_t, D3D12_VERTEX_BUFFER_VIEW> bufferViews;

	for (size_t i = 0; i < buffers.size(); i++)
	{
		D3D12Buffer *d3dbuffer = static_cast<D3D12Buffer*>(buffers[i]);
		const VertexInputBinding &binding = cxt_currentGraphicsPipeline->gfxPipelineInfo.vertexInputInfo.vertexInputBindings[firstBinding + i];

		for (size_t l = 0; l < cxt_currentGraphicsPipeline->gfxPipelineInfo.vertexInputInfo.vertexInputAttribs.size(); l++)
		{
			const VertexInputAttribute &attrib = cxt_currentGraphicsPipeline->gfxPipelineInfo.vertexInputInfo.vertexInputAttribs[l];
			
			if (attrib.binding == firstBinding + i)
			{
				D3D12_VERTEX_BUFFER_VIEW view = {};
				view.BufferLocation = d3dbuffer->bufferResource->GetGPUVirtualAddress() + offsets[i];
				view.SizeInBytes = d3dbuffer->bufferSize;
				view.StrideInBytes = binding.stride;

				bufferViews[attrib.location] = view;
			}
		}
	}

	for (auto bufferViewsIt = bufferViews.begin(); bufferViewsIt != bufferViews.end();)
	{
		std::vector<D3D12_VERTEX_BUFFER_VIEW> locationBufferViews;

		uint32_t firstLocation = bufferViewsIt->first;
		uint32_t currentLocation = firstLocation;
		
		locationBufferViews.push_back(bufferViewsIt->second);
		bufferViewsIt++;

		while (bufferViewsIt != bufferViews.end() && bufferViewsIt->first == currentLocation - 1)
		{
			currentLocation = bufferViewsIt->first;

			locationBufferViews.push_back(bufferViewsIt->second);
			bufferViewsIt++;
		}

		cmdList->IASetVertexBuffers(firstLocation, (UINT)locationBufferViews.size(), locationBufferViews.data());
	}
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

void D3D12CommandBuffer::pushConstants(uint32_t offset, uint32_t size, const void *data)
{
	cmdList->SetGraphicsRoot32BitConstants(0, (uint32_t) std::ceil(size / 4.0), data, (uint32_t) std::ceil(offset / 4.0));
}

void D3D12CommandBuffer::bindDescriptorSets(PipelineBindPoint point, uint32_t firstSet, const std::vector<DescriptorSet> &sets)
{
	uint32_t baseRootParameter = 0;

	if (cxt_currentGraphicsPipeline->gfxPipelineInfo.inputPushConstants.size > 0)
		baseRootParameter++;

	for (uint32_t i = 0; i < firstSet; i++)
	{
		cxt_currentGraphicsPipeline->gfxPipelineInfo.inputSetLayouts[i];

		if (cxt_currentGraphicsPipeline->gfxPipelineInfo.inputSetLayouts[i].samplerDescriptorCount > 0)
			baseRootParameter++;

		uint32_t srvUavCbvDescCount = cxt_currentGraphicsPipeline->gfxPipelineInfo.inputSetLayouts[i].constantBufferDescriptorCount + cxt_currentGraphicsPipeline->gfxPipelineInfo.inputSetLayouts[i].inputAttachmentDescriptorCount + cxt_currentGraphicsPipeline->gfxPipelineInfo.inputSetLayouts[i].sampledTextureDescriptorCount;

		if (srvUavCbvDescCount > 0)
			baseRootParameter++;
	}

	for (size_t i = 0; i < sets.size(); i++)
	{
		D3D12DescriptorSet *d3dset = static_cast<D3D12DescriptorSet*>(sets[i]);

		ID3D12DescriptorHeap *samplerHeapToBind = d3dset->samplerCount == 0 ? ctx_currentBoundSamplerDescHeap : d3dset->samplerHeap;
		ID3D12DescriptorHeap *srvUavCbvHeapToBind = d3dset->srvUavCbvDescriptorCount == 0 ? ctx_currentBoundSrvUavCbvDescHeap : d3dset->srvUavCbvHeap;
		ID3D12DescriptorHeap *heaps[2] = {samplerHeapToBind, srvUavCbvHeapToBind};

		if (samplerHeapToBind == nullptr)
			cmdList->SetDescriptorHeaps(1, &heaps[1]);
		else if (srvUavCbvHeapToBind == nullptr)
			cmdList->SetDescriptorHeaps(1, &heaps[0]);
		else
			cmdList->SetDescriptorHeaps(2, &heaps[0]);

		if (d3dset->srvUavCbvDescriptorCount > 0)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE descHandle = d3dset->srvUavCbvHeap->GetGPUDescriptorHandleForHeapStart();
			descHandle.ptr += renderer->cbvSrvUavDescriptorSize * (d3dset->srvUavCbvStartDescriptorSlot);

			cmdList->SetGraphicsRootDescriptorTable(baseRootParameter, descHandle);

			baseRootParameter++;
		}

		if (d3dset->samplerCount > 0)
		{
			D3D12_GPU_DESCRIPTOR_HANDLE descHandle = d3dset->samplerHeap->GetGPUDescriptorHandleForHeapStart();
			descHandle.ptr += renderer->samplerDescriptorSize * (d3dset->samplerStartDescriptorSlot);

			cmdList->SetGraphicsRootDescriptorTable(baseRootParameter, descHandle);

			baseRootParameter++;
		}
	}
}

void D3D12CommandBuffer::transitionTextureLayout(Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource)
{
#if D3D12_DEBUG_COMPATIBILITY_CHECKS

	if (newLayout == TEXTURE_LAYOUT_INITIAL_STATE)
	{
		Log::get()->error("D3D12CommandBuffer: Cannot transition a texture layout into it's initial state (TEXTURE_LAYOUT_INITIAL_STATE). This state can be transitioned out of right after the texture has been initialized but has no other use");
		throw std::runtime_error("d3d12 error: invalid newLayout in resource barrier");
	}

#endif

	D3D12Texture *d3dtexture = static_cast<D3D12Texture*>(texture);

	std::vector<D3D12_RESOURCE_BARRIER> barriers;

	for (uint32_t layer = subresource.baseArrayLayer; layer < subresource.baseArrayLayer + subresource.layerCount; layer++)
	{
		for (uint32_t mip = subresource.baseMipLevel; mip < subresource.baseMipLevel + subresource.levelCount; mip++)
		{
			D3D12_RESOURCE_TRANSITION_BARRIER transBarrier = {};
			transBarrier.pResource = d3dtexture->textureResource;
			transBarrier.Subresource = layer * d3dtexture->mipCount + mip;
			transBarrier.StateBefore = TextureLayoutToD3D12ResourceStates(oldLayout);
			transBarrier.StateAfter = TextureLayoutToD3D12ResourceStates(newLayout);

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition = transBarrier;

			barriers.push_back(barrier);
		}
	}

	cmdList->ResourceBarrier((UINT) barriers.size(), barriers.data());
}

void D3D12CommandBuffer::stageBuffer(StagingBuffer stagingBuffer, Buffer dstBuffer)
{
	D3D12StagingBuffer *d3dstagingBuffer = static_cast<D3D12StagingBuffer*>(stagingBuffer);
	D3D12Buffer *d3ddstBuffer = static_cast<D3D12Buffer*>(dstBuffer);

	DEBUG_ASSERT(d3ddstBuffer->canBeTransferDst);

	D3D12_RESOURCE_STATES stateBefore;

	switch (d3ddstBuffer->usage)
	{
		case BUFFER_USAGE_CONSTANT_BUFFER:
		case BUFFER_USAGE_VERTEX_BUFFER:
			stateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
			break;
		case BUFFER_USAGE_INDEX_BUFFER:
			stateBefore = D3D12_RESOURCE_STATE_INDEX_BUFFER;
			break;
		case BUFFER_USAGE_INDIRECT_BUFFER:
			stateBefore = D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
			break;
		default:
			stateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	D3D12_RESOURCE_TRANSITION_BARRIER tbarrier0 = {};
	tbarrier0.pResource = d3ddstBuffer->bufferResource;
	tbarrier0.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	tbarrier0.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
	tbarrier0.StateBefore = stateBefore;

	D3D12_RESOURCE_BARRIER barrier0 = {};
	barrier0.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier0.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier0.Transition = tbarrier0;
	
	D3D12_RESOURCE_TRANSITION_BARRIER tbarrier1 = {};
	tbarrier1.pResource = d3ddstBuffer->bufferResource;
	tbarrier1.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	tbarrier1.StateAfter = stateBefore;
	tbarrier1.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;

	D3D12_RESOURCE_BARRIER barrier1 = {};
	barrier1.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier1.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier1.Transition = tbarrier1;

	cmdList->ResourceBarrier(1, &barrier0);
	cmdList->CopyBufferRegion(d3ddstBuffer->bufferResource, 0, d3dstagingBuffer->bufferResource, 0, std::min<uint32_t>(d3dstagingBuffer->bufferSize, d3ddstBuffer->bufferSize));
	cmdList->ResourceBarrier(1, &barrier1);
}

void D3D12CommandBuffer::stageTextureSubresources(StagingTexture stagingTexture, Texture dstTexture, TextureSubresourceRange subresources)
{
	D3D12StagingTexture *d3dstagingTexture = static_cast<D3D12StagingTexture*>(stagingTexture);
	D3D12Texture *d3ddstTexture = static_cast<D3D12Texture*>(dstTexture);

	for (uint32_t layer = subresources.baseArrayLayer; layer < subresources.baseArrayLayer + subresources.layerCount; layer++)
	{
		for (uint32_t level = subresources.baseMipLevel; level < subresources.baseMipLevel + subresources.levelCount; level++)
		{
			uint32_t subresourceIndex = layer * d3ddstTexture->mipCount + level;

			D3D12_TEXTURE_COPY_LOCATION dstCopyLoc = {};
			dstCopyLoc.pResource = d3ddstTexture->textureResource;
			dstCopyLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			dstCopyLoc.SubresourceIndex = subresourceIndex;

			D3D12_TEXTURE_COPY_LOCATION srcCopyLoc = {};
			srcCopyLoc.pResource = d3dstagingTexture->bufferResource;
			srcCopyLoc.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			srcCopyLoc.PlacedFootprint = d3dstagingTexture->placedSubresourceFootprints[subresourceIndex];

			cmdList->CopyTextureRegion(&dstCopyLoc, 0, 0, 0, &srcCopyLoc, nullptr);
		}
	}
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

#endif