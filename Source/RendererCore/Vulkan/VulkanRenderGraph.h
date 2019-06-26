#ifndef RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_
#define RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#if BUILD_VULKAN_BACKEND

#include <RendererCore/RendererRenderGraph.h>

#include <RendererCore/Vulkan/VulkanEnums.h>
#include <RendererCore/Vulkan/VulkanObjects.h>

class VulkanRenderer;

typedef struct
{
	RenderPassAttachment attachment;
	VulkanTexture *rendererTexture;
	std::string debugName;

} VulkanRenderGraphImage;

typedef struct
{
	RenderPassAttachment attachment;
	TextureView textureView;

} VulkanRenderGraphTextureView;

typedef struct
{
	RenderGraphPipelineType pipelineType;
	VkRenderPass renderPassHandle;
	VkFramebuffer framebufferHandle;
	VkSubpassContents subpassContents;
	glm::uvec3 renderArea; // {width, height, arrayLayers}

	std::vector<size_t> passIndices;
	std::vector<VkClearValue> clearValues;
	std::vector<std::string> framebufferTextureViews;

	std::vector<VkImageMemoryBarrier> beforeRenderImageBarriers;
	std::vector<VkBufferMemoryBarrier> beforeRenderBufferBarriers;
	std::vector<VkMemoryBarrier> beforeRenderMemoryBarriers;

	std::vector<VkImageMemoryBarrier> afterRenderImageBarriers;
	std::vector<VkBufferMemoryBarrier> afterRenderBufferBarriers;
	std::vector<VkMemoryBarrier> afterRenderMemoryBarriers;

} VulkanRenderGraphRenderPass;

class VulkanRenderGraph : public RendererRenderGraph
{
	public:

	VulkanRenderGraph(VulkanRenderer *rendererPtr);
	virtual ~VulkanRenderGraph();

	Semaphore execute(bool returnWaitableSemaphore);

	TextureView getRenderGraphOutputTextureView();

	void resizeNamedSize(const std::string &sizeName, glm::uvec2 newSize);

	private:

	VulkanRenderer *renderer;

	size_t execCounter; // A counter incremented every time the execute() method is called, used for n-buffering of the command buffers

	std::vector<CommandPool> gfxCommandPools;
	std::vector<CommandBuffer> gfxCommandBuffers;
	std::vector<Semaphore> executionDoneSemaphores;

	std::vector<VulkanRenderGraphImage> graphTextures;
	std::vector<VulkanRenderGraphRenderPass> finalRenderPasses;

	std::map<std::string, VulkanRenderGraphTextureView> graphTextureViews;
	std::map<std::string, VulkanBuffer*> graphBuffers;
	std::map<std::string, VkImageLayout> graphImageViewsInitialImageLayout;

	void assignPhysicalResources(const std::vector<size_t> &passStack);
	void finishBuild(const std::vector<size_t> &passStack);
};

#endif
#endif /* RENDERERCORE_VULKAN_VULKANRENDERGRAPH_H_ */