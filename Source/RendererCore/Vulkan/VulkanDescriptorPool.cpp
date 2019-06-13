#include "RendererCore/Vulkan/VulkanDescriptorPool.h"
#if BUILD_VULKAN_BACKEND

#include <RendererCore/Vulkan/VulkanEnums.h>
#include <RendererCore/Vulkan/VulkanObjects.h>
#include <RendererCore/Vulkan/VulkanRenderer.h>
#include <RendererCore/Vulkan/VulkanPipelineHelper.h>

VulkanDescriptorPool::VulkanDescriptorPool(VulkanRenderer *rendererPtr, const DescriptorSetLayoutDescription &descriptorSetLayout, uint32_t poolBlockAllocSize)
{
	renderer = rendererPtr;
	descriptorSetDescription = descriptorSetLayout;
	this->poolBlockAllocSize = poolBlockAllocSize;

	uint32_t samplerCount = 0, textureCount = 0, constantBufferCount = 0, storageTextureCount = 0, storageBufferCount = 0, inputAttachmentCount = 0;

	for (size_t i = 0; i < descriptorSetLayout.bindings.size(); i++)
	{
		switch (descriptorSetLayout.bindings[i].type)
		{
			case DESCRIPTOR_TYPE_SAMPLER:
				samplerCount += descriptorSetLayout.bindings[i].arrayCount;
				break;
			case DESCRIPTOR_TYPE_SAMPLED_TEXTURE:
				textureCount += descriptorSetLayout.bindings[i].arrayCount;
				break;
			case DESCRIPTOR_TYPE_CONSTANT_BUFFER:
				constantBufferCount += descriptorSetLayout.bindings[i].arrayCount;
				break;
			case DESCRIPTOR_TYPE_STORAGE_TEXTURE:
				storageTextureCount += descriptorSetLayout.bindings[i].arrayCount;
				break;
			case DESCRIPTOR_TYPE_STORAGE_BUFFER:
				storageBufferCount += descriptorSetLayout.bindings[i].arrayCount;
				break;
			case DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				inputAttachmentCount += descriptorSetLayout.bindings[i].arrayCount;
				break;
		}
	}

	if (samplerCount > 0)
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.descriptorCount = samplerCount * poolBlockAllocSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLER;

		vulkanPoolSizes.push_back(poolSize);
	}

	if (constantBufferCount > 0)
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.descriptorCount = constantBufferCount * poolBlockAllocSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

		vulkanPoolSizes.push_back(poolSize);
	}

	if (inputAttachmentCount > 0)
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.descriptorCount = inputAttachmentCount * poolBlockAllocSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;

		vulkanPoolSizes.push_back(poolSize);
	}

	if (textureCount > 0)
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.descriptorCount = textureCount * poolBlockAllocSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;

		vulkanPoolSizes.push_back(poolSize);
	}

	if (storageBufferCount > 0)
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.descriptorCount = storageBufferCount * poolBlockAllocSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

		vulkanPoolSizes.push_back(poolSize);
	}

	if (storageTextureCount > 0)
	{
		VkDescriptorPoolSize poolSize = {};
		poolSize.descriptorCount = storageTextureCount * poolBlockAllocSize;
		poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;

		vulkanPoolSizes.push_back(poolSize);
	}

	descriptorPools.push_back(createDescPoolObject());
}

VulkanDescriptorPool::~VulkanDescriptorPool()
{
	for (size_t i = 0; i < descriptorPools.size(); i++)
	{
		vkDestroyDescriptorPool(renderer->device, descriptorPools[i].pool, nullptr);

		for (VulkanDescriptorSet *set : descriptorPools[i].usedPoolSets)
			delete set;

		for (std::pair<VulkanDescriptorSet*, bool> set : descriptorPools[i].unusedPoolSets)
			if (set.second && set.first != nullptr)
				delete set.first;
	}
}

DescriptorSet VulkanDescriptorPool::allocateDescriptorSet ()
{
	return allocateDescriptorSets(1)[0];
}

bool VulkanDescriptorPool::tryAllocFromDescriptorPoolObject (uint32_t poolObjIndex, VulkanDescriptorSet* &outSet)
{
	VulkanDescriptorPoolObject &poolObj = descriptorPools[poolObjIndex];

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

		VkDescriptorSetLayout descLayout = renderer->pipelineHandler->createDescriptorSetLayout(descriptorSetDescription);

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
			if (!tryAllocFromDescriptorPoolObject(p, vulkanSet))
			{
				break;
			}
		}

		// If the set is still nullptr, then we'll have to create a new pool to allocate from
		if (vulkanSet == nullptr)
		{
			descriptorPools.push_back(createDescPoolObject());

			tryAllocFromDescriptorPoolObject(descriptorPools.size() - 1, vulkanSet);
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

VulkanDescriptorPoolObject VulkanDescriptorPool::createDescPoolObject()
{
	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(vulkanPoolSizes.size());
	poolInfo.pPoolSizes = vulkanPoolSizes.data();
	poolInfo.maxSets = poolBlockAllocSize;

	VulkanDescriptorPoolObject poolObject = {};
	poolObject.usedPoolSets.reserve(poolBlockAllocSize);
	poolObject.unusedPoolSets.reserve(poolBlockAllocSize);

	VK_CHECK_RESULT(vkCreateDescriptorPool(renderer->device, &poolInfo, nullptr, &poolObject.pool));

	for (uint32_t i = 0; i < poolBlockAllocSize; i++)
	{
		poolObject.unusedPoolSets.push_back(std::make_pair((VulkanDescriptorSet*) nullptr, false));
	}

	return poolObject;
}
#endif