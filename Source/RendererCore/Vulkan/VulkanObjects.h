/*
 * VulkanObjects.h
 *
 *  Created on: Sep 15, 2017
 *      Author: david
 */

#ifndef RENDERING_VULKAN_VULKANOBJECTS_H_
#define RENDERING_VULKAN_VULKANOBJECTS_H_

#include <RendererCore/Vulkan/vulkan_common.h>

#include <set>

struct VulkanTexture : public RendererTexture
{
		VkImage imageHandle;
		VmaAllocation imageMemory;

		VkFormat imageFormat;
};

struct VulkanTextureView : public RendererTextureView
{
		VkImageView imageView;
};

struct VulkanSampler : public RendererSampler
{
		VkSampler samplerHandle;
};

struct VulkanRenderPass : public RendererRenderPass
{
		VkRenderPass renderPassHandle;
};

struct VulkanFramebuffer : public RendererFramebuffer
{
		VkFramebuffer framebufferHandle;
};

struct VulkanPipeline : public RendererPipeline
{
		VkPipeline pipelineHandle;
		VkPipelineLayout pipelineLayoutHandle;
};

struct VulkanDescriptorSet : public RendererDescriptorSet
{
		VkDescriptorSet setHandle;
		uint32_t descriptorPoolObjectIndex; // The index of "VulkanDescriptorPool::descriptorPools" that this set was allocated from
};

/*
 struct VulkanDescriptorSetLayout : public RendererDescriptorSetLayout
 {
 VkDescriptorSetLayout setLayoutHandle;
 };
 */

struct VulkanShaderModule : public RendererShaderModule
{
		VkShaderModule module;
};

struct VulkanStagingBuffer : public RendererStagingBuffer
{
		VkBuffer bufferHandle;
		VmaAllocation bufferMemory;
};

struct VulkanStagingTexture : public RendererStagingTexture
{
	VkBuffer bufferHandle;
	VmaAllocation bufferMemory;

	std::vector<uint64_t> subresourceOffets;
	std::vector<uint64_t> subresourceRowSize;
	std::vector<uint64_t> subresourceRowPitches;

};

struct VulkanBuffer : public RendererBuffer
{
		VkBuffer bufferHandle;
		VmaAllocation bufferMemory;
		VkDeviceSize memorySize;
};

struct VulkanFence : public RendererFence
{
		VkFence fenceHandle;
};

struct VulkanSemaphore : public RendererSemaphore
{
		VkSemaphore semHandle;
};

// Data store for each VkPipelineLayout object in the cache to compare against others
typedef struct VulkanPipelineLayoutCacheInfo
{
		VkPipelineLayoutCreateFlags flags;
		std::vector<VkDescriptorSetLayout> setLayouts;
		std::vector<VkPushConstantRange> pushConstantRanges;
} VulkanPipelineLayoutCacheInfo;

class DeviceQueues
{
	public:
		int graphicsFamily = -1;
		int presentFamily = -1;
		int computeFamily = -1;
		int transferFamily = -1;

		uint32_t graphicsQueue;
		uint32_t presentQueue;
		uint32_t computeQueue;
		uint32_t transferQueue;

		std::vector<uint32_t> uniqueQueueFamilies;
		std::vector<uint32_t> uniqueQueueFamilyQueueCount;

		bool isComplete ()
		{
			return graphicsFamily >= 0 && presentFamily >= 0 && transferFamily >= 0 && computeFamily >= 0;
		}

		bool hasUniquePresentFamily ()
		{
			return graphicsFamily != presentFamily;
		}

		bool hasUniqueComputeFamily ()
		{
			return graphicsFamily != computeFamily;
		}

		bool hasUniqueTransferFamily ()
		{
			return graphicsFamily != transferFamily;
		}

		void createUniqueQueueFamilyList ()
		{
			std::set<uint32_t> uniqueSet = {static_cast<uint32_t>(graphicsFamily), static_cast<uint32_t>(presentFamily), static_cast<uint32_t>(transferFamily), static_cast<uint32_t>(computeFamily)};

			uniqueQueueFamilies = std::vector<uint32_t>(uniqueSet.begin(), uniqueSet.end());
			std::map<uint32_t, uint32_t> queueCount;

			for (uint32_t i : uniqueQueueFamilies)
				queueCount[i] = 1;

			queueCount[graphicsFamily] = std::max(queueCount[graphicsFamily], graphicsQueue + 1);
			queueCount[computeFamily] = std::max(queueCount[computeFamily], computeQueue + 1);
			queueCount[transferFamily] = std::max(queueCount[transferFamily], transferQueue + 1);
			queueCount[presentFamily] = std::max(queueCount[presentFamily], presentQueue + 1);

			for (auto it : queueCount)
			{
				uniqueQueueFamilyQueueCount.push_back(it.second);
			}
		}

};

#include <RendererCore/Vulkan/VulkanCommandBuffer.h>
#include <RendererCore/Vulkan/VulkanCommandPool.h>
#include <RendererCore/Vulkan/VulkanDescriptorPool.h>

#endif /* RENDERING_VULKAN_VULKANOBJECTS_H_ */
