#ifndef RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_
#define RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#include <RendererCore/RendererRenderGraph.h>

typedef struct
{
	RenderPassAttachment attachment;
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

class VulkanRenderGraph : public RendererRenderGraph
{
public:
	VulkanRenderGraph(Renderer *rendererPtr);
	virtual ~VulkanRenderGraph();

	Semaphore execute();

	void resizeNamedSize(const std::string &sizeName, glm::uvec2 newSize);

	void assignPhysicalResources(const std::vector<size_t> &passStack);
	void finishBuild(const std::vector<size_t> &passStack);

	TextureView getRenderGraphOutputTextureView();

private:

	std::vector<VulkanRenderGraphImage> graphImages;
	std::map<std::string, size_t> graphImagesMap;

	std::map<std::string, TextureView> resourceImageViews;
	std::map<std::string, RenderPassOutputAttachment> allAttachments;

	std::vector<std::pair<VulkanRenderGraphOpCode, void*>> execCodes;

	std::vector<std::unique_ptr<VulkanRenderGraphBeginRenderPassData>> beginRenderPassOpsData;
	std::vector<std::unique_ptr<VulkanRenderGraphPostBlitOpData>> postBlitOpsData;
	std::vector<std::unique_ptr<VulkanRenderGraphCallRenderFuncOpData>> callRenderFuncOpsData;

	std::vector<CommandPool> gfxCommandPools;
	std::vector<Semaphore> executionDoneSemaphores;

	size_t execCounter; // A counter incremented every time the execute() method is called, used for n-buffering of the command buffers

	void cleanupResources();
};

#endif /* RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_ */