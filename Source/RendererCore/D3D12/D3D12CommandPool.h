#ifndef RENDERING_D3D12_D3D12COMMANDPOOL_H_
#define RENDERING_D3D12_D3D12COMMANDPOOL_H_

#include <RendererCore/D3D12/d3d12_common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class D3D12CommandBuffer;

class D3D12CommandPool : public RendererCommandPool
{
	public:

	D3D12CommandPool(ID3D12Device2 *devicePtr, QueueType queueType);
	virtual ~D3D12CommandPool();

	RendererCommandBuffer *allocateCommandBuffer(CommandBufferLevel level);
	std::vector<RendererCommandBuffer*> allocateCommandBuffers(CommandBufferLevel level, uint32_t commandBufferCount);

	void resetCommandPoolAndFreeCommandBuffer(RendererCommandBuffer *commandBuffer);
	void resetCommandPoolAndFreeCommandBuffers(const std::vector<RendererCommandBuffer*> &commandBuffers);

	void resetCommandPool();

	private:

	ID3D12Device2 *device;
	ID3D12CommandAllocator *cmdAlloc;
	ID3D12CommandAllocator *bundleCmdAlloc;

	std::vector<D3D12CommandBuffer*> allocatedCmdLists;
	D3D12_COMMAND_LIST_TYPE cmdListType;

	friend class D3D12CommandBuffer;
};

#endif /* RENDERING_D3D12_D3D12COMMANDPOOL_H_*/