
#ifndef RENDERING_VULKAN_VULKANPIPELINES_H_
#define RENDERING_VULKAN_VULKANPIPELINES_H_

#include <RendererCore/Renderer.h>
#include <RendererCore/Vulkan/vulkan_common.h>
#include <RendererCore/Vulkan/VulkanObjects.h>

class VulkanRenderer;

class VulkanPipelines
{
	public:


		VulkanPipelines (VulkanRenderer *parentVulkanRenderer);
		virtual ~VulkanPipelines ();

		Pipeline createGraphicsPipeline (const GraphicsPipelineInfo &pipelineInfo, RenderPass renderPass, uint32_t subpass);
		Pipeline createComputePipeline(const ComputePipelineInfo &pipelineInfo);

		VkDescriptorSetLayout createDescriptorSetLayout (const std::vector<DescriptorSetLayoutBinding> &layoutBindings);
		VkDescriptorSetLayout createDescriptorSetLayout (const VkDescriptorSetLayoutCreateInfo &setLayoutInfo);

	private:

		VulkanRenderer *renderer;

		// I'm also letting the renderer backend handle descriptor set layout caches, so the front end only gives the layout info and gets it easy
		std::vector<std::pair<VulkanDescriptorSetLayoutCacheInfo, VkDescriptorSetLayout> > descriptorSetLayoutCache;

		/*
		 * All of these functions are converter functions for the generic renderer data to vulkan renderer data. Note that
		 * all of these functions do not rely on pointers, so the vulkan structures that use pointers are not included here.
		 */
		std::vector<VkPipelineShaderStageCreateInfo> getPipelineShaderStages (const std::vector<PipelineShaderStage> &stages);
		VkPipelineInputAssemblyStateCreateInfo getPipelineInputAssemblyInfo (const PipelineInputAssemblyInfo &info);
		VkPipelineRasterizationStateCreateInfo getPipelineRasterizationInfo (const PipelineRasterizationInfo &info);
		VkPipelineDepthStencilStateCreateInfo getPipelineDepthStencilInfo (const PipelineDepthStencilInfo &info);
};

#endif /* RENDERING_VULKAN_VULKANPIPELINES_H_ */
