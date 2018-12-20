
#ifndef RENDERING_VULKAN_VULKANCOMMANDPOOL_H_
#define RENDERING_VULKAN_VULKANCOMMANDPOOL_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class VulkanCommandPool : public RendererCommandPool
{
	public:
		VkCommandPool poolHandle;
		VkDevice device;

		virtual ~VulkanCommandPool ();

		RendererCommandBuffer *allocateCommandBuffer (CommandBufferLevel level);
		std::vector<RendererCommandBuffer*> allocateCommandBuffers (CommandBufferLevel level, uint32_t commandBufferCount);

		void freeCommandBuffer (RendererCommandBuffer *commandBuffer);
		void freeCommandBuffers (const std::vector<RendererCommandBuffer*> &commandBuffers);

		void resetCommandPool (bool releaseResources);

	private:
};

#endif /* RENDERING_VULKAN_VULKANCOMMANDPOOL_H_ */
