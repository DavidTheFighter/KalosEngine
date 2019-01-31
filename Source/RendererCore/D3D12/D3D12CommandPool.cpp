#include "RendererCore/D3D12/D3D12CommandPool.h"
#if BUILD_D3D12_BACKEND

#include <RendererCore/D3D12/D3D12Renderer.h>
#include <RendererCore/D3D12/D3D12CommandBuffer.h>
#include <RendererCore/D3D12/D3D12Enums.h>

D3D12CommandPool::D3D12CommandPool(D3D12Renderer *rendererPtr, QueueType queueType)
{
	renderer = rendererPtr;
	cmdListType = queueTypeToD3D12CommandListType(queueType);
	
	DX_CHECK_RESULT(renderer->device->CreateCommandAllocator(cmdListType, IID_PPV_ARGS(&cmdAlloc)));
	DX_CHECK_RESULT(renderer->device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&bundleCmdAlloc)));
}

D3D12CommandPool::~D3D12CommandPool()
{
	for (size_t i = 0; i < allocatedCmdLists.size(); i++)
		delete allocatedCmdLists[i];

	cmdAlloc->Release();
	bundleCmdAlloc->Release();
}

RendererCommandBuffer *D3D12CommandPool::allocateCommandBuffer(CommandBufferLevel level)
{
	return allocateCommandBuffers(level, 1)[0];
}

std::vector<RendererCommandBuffer*> D3D12CommandPool::allocateCommandBuffers(CommandBufferLevel level, uint32_t commandBufferCount)
{
	std::vector<RendererCommandBuffer*> cmdBuffers;

	for (uint32_t i = 0; i < commandBufferCount; i++)
	{
		D3D12CommandBuffer *cmdBuffer = new D3D12CommandBuffer(renderer, this, level == COMMAND_BUFFER_LEVEL_PRIMARY ? cmdListType : D3D12_COMMAND_LIST_TYPE_BUNDLE);

		cmdBuffers.push_back(cmdBuffer);
		allocatedCmdLists.push_back(cmdBuffer);
	}

	return cmdBuffers;
}

void D3D12CommandPool::resetCommandPoolAndFreeCommandBuffer(RendererCommandBuffer *commandBuffer)
{
	resetCommandPoolAndFreeCommandBuffers({commandBuffer});
}

void D3D12CommandPool::resetCommandPoolAndFreeCommandBuffers(const std::vector<RendererCommandBuffer*> &commandBuffers)
{
	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		auto it = std::find(allocatedCmdLists.begin(), allocatedCmdLists.end(), static_cast<D3D12CommandBuffer*>(commandBuffers[i]));

		if (it != allocatedCmdLists.end())
			allocatedCmdLists.erase(it);

		delete commandBuffers[i];
	}
}

void D3D12CommandPool::resetCommandPool()
{
	for (size_t i = 0; i < allocatedCmdLists.size(); i++)
		static_cast<D3D12CommandBuffer*>(allocatedCmdLists[i])->startedRecording = false;

	cmdAlloc->Reset();
	bundleCmdAlloc->Reset();
}
#endif