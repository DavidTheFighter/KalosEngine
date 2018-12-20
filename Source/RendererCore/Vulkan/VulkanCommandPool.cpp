
#include "RendererCore/Vulkan/VulkanCommandPool.h"

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

	for (uint32_t i = 0; i < commandBufferCount; i ++)
	{
		VulkanCommandBuffer* vkCmdBuffer = new VulkanCommandBuffer();
		vkCmdBuffer->level = level;
		vkCmdBuffer->bufferHandle = vkCommandBuffers[i];

		commandBuffers.push_back(vkCmdBuffer);
	}

	return commandBuffers;
}

void VulkanCommandPool::freeCommandBuffer (RendererCommandBuffer *commandBuffer)
{
	freeCommandBuffers({commandBuffer});
}

void VulkanCommandPool::freeCommandBuffers (const std::vector<RendererCommandBuffer*> &commandBuffers)
{
	std::vector<VkCommandBuffer> vkCmdBuffers;

	for (size_t i = 0; i < commandBuffers.size(); i ++)
	{
		VulkanCommandBuffer *cmdBuffer = static_cast<VulkanCommandBuffer*>(commandBuffers[i]);

		vkCmdBuffers.push_back(cmdBuffer->bufferHandle);

		// We can delete these here because the rest of the function only relies on the vulkan handles
		delete cmdBuffer;
	}

	vkFreeCommandBuffers(device, poolHandle, static_cast<uint32_t>(vkCmdBuffers.size()), vkCmdBuffers.data());
}

void VulkanCommandPool::resetCommandPool (bool releaseResources)
{
	VK_CHECK_RESULT(vkResetCommandPool(device, poolHandle, releaseResources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0))
}
