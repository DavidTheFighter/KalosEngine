#ifndef RENDERING_VULKAN_VULKANPIPELINEHELPER_H_
#define RENDERING_VULKAN_VULKANPIPELINEHELPER_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#if BUILD_VULKAN_BACKEND

#include <RendererCore/Renderer.h>
#include <RendererCore/Vulkan/VulkanObjects.h>

class VulkanRenderer;

typedef struct
{
	DescriptorSetLayoutDescription description;
	VkDescriptorSetLayout setLayoutHandle;

} VulkanDescriptorSetLayoutCacheInfo;

typedef struct
{
	DescriptorStaticSampler samplerDesc;
	VkSampler samplerHandle;

} VulkanDescriptorSetStaticSamplerCache;

class VulkanPipelineHelper
{
	public:

		VulkanPipelineHelper (VulkanRenderer *parentVulkanRenderer);
		virtual ~VulkanPipelineHelper ();

		Pipeline createGraphicsPipeline (const GraphicsPipelineInfo &pipelineInfo, RenderPass renderPass, uint32_t subpass);
		Pipeline createComputePipeline(const ComputePipelineInfo &pipelineInfo);

		VkDescriptorSetLayout createDescriptorSetLayout (const DescriptorSetLayoutDescription &setDescription);

	private:

		VulkanRenderer *renderer;

		// I'm also letting the renderer backend handle descriptor set layout caches, so the front end only gives the layout info and gets it easy
		std::vector<VulkanDescriptorSetLayoutCacheInfo> descriptorSetLayoutCache;

		std::vector<VulkanDescriptorSetStaticSamplerCache> staticSamplerCache;

		/*
		 * All of these functions are converter functions for the generic renderer data to vulkan renderer data. Note that
		 * all of these functions do not rely on pointers, so the vulkan structures that use pointers are not included here.
		 */
		std::vector<VkPipelineShaderStageCreateInfo> getPipelineShaderStages (const std::vector<PipelineShaderStage> &stages);
		VkPipelineInputAssemblyStateCreateInfo getPipelineInputAssemblyInfo (const PipelineInputAssemblyInfo &info);
		VkPipelineRasterizationStateCreateInfo getPipelineRasterizationInfo (const PipelineRasterizationInfo &info);
		VkPipelineDepthStencilStateCreateInfo getPipelineDepthStencilInfo (const PipelineDepthStencilInfo &info);
};

#endif
#endif /* RENDERING_VULKAN_VULKANPIPELINES_H_ */
