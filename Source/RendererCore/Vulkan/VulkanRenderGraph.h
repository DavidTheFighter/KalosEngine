#ifndef RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_
#define RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#if BUILD_VULKAN_BACKEND

#include <RendererCore/RendererRenderGraph.h>

#include <RendererCore/Vulkan/VulkanEnums.h>
#include <RendererCore/Vulkan/VulkanObjects.h>

typedef struct
{
	//RenderPassAttachment attachment;
	VkImageUsageFlags usageFlags;

	VkImage imageHandle;
	VmaAllocation imageMemory;

} VulkanRenderGraphImage;

typedef enum
{
	VKRG_OPCODE_BEGIN_RENDER_PASS,
	VKRG_OPCODE_END_RENDER_PASS,
	VKRG_OPCODE_NEXT_SUBPASS,
	VKRG_OPCODE_POST_BLIT,
	VKRG_OPCODE_CALL_RENDER_FUNC,
	VKRG_OPCODE_MAX_ENUM
} VulkanRenderGraphOpCode;

typedef struct
{
	VkRenderPass renderPass;
	VkFramebuffer framebuffer;
	suvec3 size; // {width, height, arrayLayers}
	std::vector<VkClearValue> clearValues;
	VkSubpassContents subpassContents;

} VulkanRenderGraphBeginRenderPassData;

typedef struct
{
	size_t graphImageIndex;
} VulkanRenderGraphPostBlitOpData;

typedef struct
{
	size_t passIndex;
	uint32_t counter;
} VulkanRenderGraphCallRenderFuncOpData;

class VulkanRenderer;

class VulkanRenderGraph : public RendererRenderGraph
{
	public:

	VulkanRenderGraph(VulkanRenderer *rendererPtr);
	virtual ~VulkanRenderGraph();

	Semaphore execute(bool returnWaitableSemaphore);

	virtual TextureView getRenderGraphOutputTextureView();

	protected:

	virtual void assignPhysicalResources(const std::vector<size_t> &passStack);
	virtual void finishBuild(const std::vector<size_t> &passStack);

	private:

	VulkanRenderer *renderer;

	void cleanupResources();
};

#endif
#endif /* RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_ */