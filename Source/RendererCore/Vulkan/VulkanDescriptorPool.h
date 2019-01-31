#ifndef RENDERING_VULKAN_VULKANDESCRIPTORPOOL_H_
#define RENDERING_VULKAN_VULKANDESCRIPTORPOOL_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#if BUILD_VULKAN_BACKEND

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

	VulkanDescriptorPool(VulkanRenderer *rendererPtr, const DescriptorSetLayoutDescription &descriptorSetLayout, uint32_t poolBlockAllocSize);
	virtual ~VulkanDescriptorPool();

	DescriptorSet allocateDescriptorSet ();
	std::vector<DescriptorSet> allocateDescriptorSets (uint32_t setCount);

	void freeDescriptorSet (DescriptorSet set);
	void freeDescriptorSets (const std::vector<DescriptorSet> sets);

	private:

	VulkanRenderer *renderer;

	DescriptorSetLayoutDescription descriptorSetDescription;

	uint32_t poolBlockAllocSize; // The .maxSets value for each pool created
	std::vector<VkDescriptorPoolSize> vulkanPoolSizes; // Just so that it's faster/easier to create new vulkan descriptor pool objects

	// A list of all the local pool/blocks that have been created & their own allocation data
	std::vector<VulkanDescriptorPoolObject> descriptorPools;

	VulkanDescriptorPoolObject createDescPoolObject();

	bool tryAllocFromDescriptorPoolObject(uint32_t poolObjIndex, VulkanDescriptorSet* &outSet);
};

#endif
#endif /* RENDERING_VULKAN_VULKANDESCRIPTORPOOL_H_ */
