
#include "RendererCore/Vulkan/VulkanDescriptorPool.h"

#include <RendererCore/Vulkan/VulkanEnums.h>
#include <RendererCore/Vulkan/VulkanObjects.h>
#include <RendererCore/Vulkan/VulkanRenderer.h>
#include <RendererCore/Vulkan/VulkanPipelineHelper.h>

DescriptorSet VulkanDescriptorPool::allocateDescriptorSet ()
{
	return allocateDescriptorSets(1)[0];
}

bool VulkanRenderer_tryAllocFromDescriptorPoolObject (VulkanRenderer *renderer, VulkanDescriptorPool *pool, uint32_t poolObjIndex, VulkanDescriptorSet* &outSet)
{
	VulkanDescriptorPoolObject &poolObj = pool->descriptorPools[poolObjIndex];

	if (poolObj.unusedPoolSets.size() == 0)
		return false;

	// TODO Change vulkan descriptor sets to allocate in chunks rather than invididually

	bool setIsAllocated = poolObj.unusedPoolSets.back().second;

	if (setIsAllocated)
	{
		outSet = poolObj.unusedPoolSets.back().first;

		DEBUG_ASSERT(outSet != nullptr);

		poolObj.usedPoolSets.push_back(poolObj.unusedPoolSets.back().first);
		poolObj.unusedPoolSets.pop_back();
	}
	else
	{
		VulkanDescriptorSet *vulkanDescSet = new VulkanDescriptorSet();
		vulkanDescSet->descriptorPoolObjectIndex = poolObjIndex;

		VkDescriptorSetLayout descLayout = renderer->pipelineHandler->createDescriptorSetLayout(pool->layoutBindings);

		VkDescriptorSetAllocateInfo descSetAllocInfo = {};
		descSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descSetAllocInfo.descriptorPool = poolObj.pool;
		descSetAllocInfo.descriptorSetCount = 1;
		descSetAllocInfo.pSetLayouts = &descLayout;

		VK_CHECK_RESULT(vkAllocateDescriptorSets(renderer->device, &descSetAllocInfo, &vulkanDescSet->setHandle));

		poolObj.usedPoolSets.push_back(vulkanDescSet);
		poolObj.unusedPoolSets.pop_back();

		outSet = vulkanDescSet;
	}

	return true;
}


std::vector<DescriptorSet> VulkanDescriptorPool::allocateDescriptorSets (uint32_t setCount)
{
	DEBUG_ASSERT(setCount > 0);

	std::vector<DescriptorSet> vulkanSets = std::vector<DescriptorSet>(setCount);

	for (uint32_t i = 0; i < setCount; i ++)
	{
		//printf("-- %u --\n", i);
		VulkanDescriptorSet *vulkanSet = nullptr;

		// Iterate over each pool object to see if there's one we can alloc from
		for (size_t p = 0; p < descriptorPools.size(); p ++)
		{
			if (VulkanRenderer_tryAllocFromDescriptorPoolObject(renderer, this, p, vulkanSet))
			{
				break;
			}
		}

		// If the set is still nullptr, then we'll have to create a new pool to allocate from
		if (vulkanSet == nullptr)
		{
			descriptorPools.push_back(renderer->createDescPoolObject(vulkanPoolSizes, poolBlockAllocSize));

			VulkanRenderer_tryAllocFromDescriptorPoolObject(renderer, this, descriptorPools.size() - 1, vulkanSet);
		}

		vulkanSets[i] = vulkanSet;
	}

	return vulkanSets;
}

void VulkanDescriptorPool::freeDescriptorSet (DescriptorSet set)
{
	freeDescriptorSets({set});
}

void VulkanDescriptorPool::freeDescriptorSets (const std::vector<DescriptorSet> sets)
{
	for (size_t i = 0; i < sets.size(); i ++)
	{
		VulkanDescriptorSet *vulkanDescSet = static_cast<VulkanDescriptorSet*> (sets[i]);

		VulkanDescriptorPoolObject &poolObj = descriptorPools[vulkanDescSet->descriptorPoolObjectIndex];
		poolObj.unusedPoolSets.push_back(std::make_pair(vulkanDescSet, true));

		auto it = std::find(poolObj.usedPoolSets.begin(), poolObj.usedPoolSets.end(), vulkanDescSet);

		DEBUG_ASSERT(it != poolObj.usedPoolSets.end());

		poolObj.usedPoolSets.erase(it);
	}
}

