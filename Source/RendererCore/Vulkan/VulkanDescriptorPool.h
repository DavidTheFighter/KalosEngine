
#ifndef RENDERING_VULKAN_VULKANDESCRIPTORPOOL_H_
#define RENDERING_VULKAN_VULKANDESCRIPTORPOOL_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

struct VulkanDescriptorSet;

struct VulkanDescriptorPoolObject
{
		VkDescriptorPool pool; // This pool object's vulkan pool handle

		std::vector<std::pair<VulkanDescriptorSet*, bool> > unusedPoolSets;
		std::vector<VulkanDescriptorSet*> usedPoolSets;
};

class VulkanRenderer;

class VulkanDescriptorPool : public RendererDescriptorPool
{
	public:

		bool canFreeSetFromPool; // If individual sets can be freed, i.e. the pool was created w/ VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
		uint32_t poolBlockAllocSize; // The .maxSets value for each pool created, should not exceed 1024 (arbitrary, but usefully arbitrary :D)

		std::vector<DescriptorSetLayoutBinding> layoutBindings; // The layout bindings of each set the pool allocates
		std::vector<VkDescriptorPoolSize> vulkanPoolSizes; // Just so that it's faster/easier to create new vulkan descriptor pool objects

		// A list of all the local pool/blocks that have been created & their own allocation data
		std::vector<VulkanDescriptorPoolObject> descriptorPools;

		VulkanRenderer *renderer;

		DescriptorSet allocateDescriptorSet ();
		std::vector<DescriptorSet> allocateDescriptorSets (uint32_t setCount);

		void freeDescriptorSet (DescriptorSet set);
		void freeDescriptorSets (const std::vector<DescriptorSet> sets);
};

#endif /* RENDERING_VULKAN_VULKANDESCRIPTORPOOL_H_ */
