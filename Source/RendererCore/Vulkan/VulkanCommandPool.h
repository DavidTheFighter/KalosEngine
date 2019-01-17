
#ifndef RENDERING_VULKAN_VULKANCOMMANDPOOL_H_
#define RENDERING_VULKAN_VULKANCOMMANDPOOL_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class VulkanCommandBuffer;

class VulkanCommandPool : public RendererCommandPool
{
	public:
		VkCommandPool poolHandle;
		VkDevice device;

		virtual ~VulkanCommandPool ();

		RendererCommandBuffer *allocateCommandBuffer (CommandBufferLevel level);
		std::vector<RendererCommandBuffer*> allocateCommandBuffers (CommandBufferLevel level, uint32_t commandBufferCount);

		void resetCommandPoolAndFreeCommandBuffer (RendererCommandBuffer *commandBuffer);
		void resetCommandPoolAndFreeCommandBuffers (const std::vector<RendererCommandBuffer*> &commandBuffers);

		void resetCommandPool ();

	private:

	std::vector<VulkanCommandBuffer*> allocatedCmdLists;

};

#endif /* RENDERING_VULKAN_VULKANCOMMANDPOOL_H_ */
