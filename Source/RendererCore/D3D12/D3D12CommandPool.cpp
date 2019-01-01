#include "RendererCore/D3D12/D3D12CommandPool.h"

#include <RendererCore/D3D12/D3D12CommandBuffer.h>
#include <RendererCore/D3D12/D3D12Enums.h>

D3D12CommandPool::D3D12CommandPool(ID3D12Device2 *devicePtr, QueueType queueType)
{
	device = devicePtr;
	cmdListType = queueTypeToD3D12CommandListType(queueType);
	
	DX_CHECK_RESULT(device->CreateCommandAllocator(cmdListType, IID_PPV_ARGS(&cmdAlloc)));
	DX_CHECK_RESULT(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&bundleCmdAlloc)));
}

D3D12CommandPool::~D3D12CommandPool()
{
	cmdAlloc->Release();
	bundleCmdAlloc->Release();

	for (size_t i = 0; i < allocatedCmdLists.size(); i++)
		delete allocatedCmdLists[i];
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
		D3D12CommandBuffer *cmdBuffer = new D3D12CommandBuffer(this, level == COMMAND_BUFFER_LEVEL_PRIMARY ? cmdListType : D3D12_COMMAND_LIST_TYPE_BUNDLE);

		cmdBuffers.push_back(cmdBuffer);
		allocatedCmdLists.push_back(cmdBuffer);
	}

	return cmdBuffers;
}

void D3D12CommandPool::freeCommandBuffer(RendererCommandBuffer *commandBuffer)
{
	freeCommandBuffers({commandBuffer});
}

void D3D12CommandPool::freeCommandBuffers(const std::vector<RendererCommandBuffer*> &commandBuffers)
{
	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		auto it = std::find(allocatedCmdLists.begin(), allocatedCmdLists.end(), static_cast<D3D12CommandBuffer*>(commandBuffers[i]));

		if (it != allocatedCmdLists.end())
			allocatedCmdLists.erase(it);

		delete commandBuffers[i];
	}
}

void D3D12CommandPool::resetCommandPool(bool releaseResources)
{

}