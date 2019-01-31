#include "RendererCore/Vulkan/VulkanCommandPool.h"
#if BUILD_VULKAN_BACKEND

#include <RendererCore/Vulkan/VulkanEnums.h>
#include <RendererCore/Vulkan/VulkanObjects.h>

VulkanCommandPool::~VulkanCommandPool ()
{

}

RendererCommandBuffer *VulkanCommandPool::allocateCommandBuffer (CommandBufferLevel level)
{
	return allocateCommandBuffers(level, 1)[0];
}

std::vector<RendererCommandBuffer*> VulkanCommandPool::allocateCommandBuffers (CommandBufferLevel level, uint32_t commandBufferCount)
{
	DEBUG_ASSERT(commandBufferCount > 0);

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = commandBufferCount;
	allocInfo.commandPool = poolHandle;
	allocInfo.level = toVkCommandBufferLevel(level);

	std::vector<VkCommandBuffer> vkCommandBuffers(commandBufferCount);
	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &allocInfo, vkCommandBuffers.data()));

	std::vector<CommandBuffer> commandBuffers;
	std::vector<VulkanCommandBuffer*> vulkanAllocatedComamnds;

	for (uint32_t i = 0; i < commandBufferCount; i ++)
	{
		VulkanCommandBuffer* vkCmdBuffer = new VulkanCommandBuffer();
		vkCmdBuffer->level = level;
		vkCmdBuffer->bufferHandle = vkCommandBuffers[i];

		commandBuffers.push_back(vkCmdBuffer);
		vulkanAllocatedComamnds.push_back(vkCmdBuffer);
	}

	allocatedCmdLists.insert(allocatedCmdLists.end(), vulkanAllocatedComamnds.begin(), vulkanAllocatedComamnds.end());

	return commandBuffers;
}

void VulkanCommandPool::resetCommandPoolAndFreeCommandBuffer (RendererCommandBuffer *commandBuffer)
{
	resetCommandPoolAndFreeCommandBuffers({commandBuffer});
}

void VulkanCommandPool::resetCommandPoolAndFreeCommandBuffers (const std::vector<RendererCommandBuffer*> &commandBuffers)
{
	resetCommandPool();

	std::vector<VkCommandBuffer> vkCmdBuffers;

	for (size_t i = 0; i < commandBuffers.size(); i ++)
	{
		VulkanCommandBuffer *cmdBuffer = static_cast<VulkanCommandBuffer*>(commandBuffers[i]);

		vkCmdBuffers.push_back(cmdBuffer->bufferHandle);

		auto it = std::find(allocatedCmdLists.begin(), allocatedCmdLists.end(), cmdBuffer);

		if (it != allocatedCmdLists.end())
			allocatedCmdLists.erase(it);

		// We can delete these here because the rest of the function only relies on the vulkan handles
		delete cmdBuffer;
	}

	vkFreeCommandBuffers(device, poolHandle, static_cast<uint32_t>(vkCmdBuffers.size()), vkCmdBuffers.data());
}

void VulkanCommandPool::resetCommandPool ()
{
	VK_CHECK_RESULT(vkResetCommandPool(device, poolHandle, 0));
}

#endif