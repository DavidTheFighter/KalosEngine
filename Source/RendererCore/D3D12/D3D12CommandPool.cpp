#include "RendererCore/D3D12/D3D12CommandPool.h"

#include <RendererCore/D3D12/D3D12CommandBuffer.h>

D3D12CommandPool::D3D12CommandPool(ID3D12Device2 *devicePtr, QueueType queueType)
{
	device = devicePtr;
	
	DX_CHECK_RESULT(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&cmdAlloc)));
}

D3D12CommandPool::~D3D12CommandPool()
{

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
		D3D12CommandBuffer *cmdBuffer = new D3D12CommandBuffer();

		cmdBuffers.push_back(cmdBuffer);
	}

	return cmdBuffers;
}

void D3D12CommandPool::freeCommandBuffer(RendererCommandBuffer *commandBuffer)
{

}

void D3D12CommandPool::freeCommandBuffers(const std::vector<RendererCommandBuffer*> &commandBuffers)
{

}

void D3D12CommandPool::resetCommandPool(bool releaseResources)
{

}