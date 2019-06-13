#include "RendererCore/D3D12/D3D12DescriptorPool.h"
#if BUILD_D3D12_BACKEND

#include <RendererCore/D3D12/D3D12Renderer.h>

#include <RendererCore/D3D12/D3D12Objects.h>

D3D12DescriptorPool::D3D12DescriptorPool(D3D12Renderer *rendererPtr, const DescriptorSetLayoutDescription &descriptorSetLayout)
{
	renderer = rendererPtr;
	this->descriptorSetLayout = descriptorSetLayout;

	srvuavcbvDescriptorCount = 0;
	samplerDescriptorCount = 0;
	nonStaticSamplerDescriptorCount = 0;

	std::map<D3D12_DESCRIPTOR_RANGE_TYPE, std::map<uint32_t, DescriptorSetBinding>> bindingArrayCountMap;

	for (size_t i = 0; i < descriptorSetLayout.bindings.size(); i++)
	{
		const DescriptorSetBinding &binding = descriptorSetLayout.bindings[i];

		switch (binding.type)
		{
			case DESCRIPTOR_TYPE_SAMPLER:
				samplerDescriptorCount += binding.arrayCount;
				bindingArrayCountMap[D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER][binding.binding] = binding;

				if (binding.staticSamplers.size() == 0)
					nonStaticSamplerDescriptorCount += binding.arrayCount;
				break;
			case DESCRIPTOR_TYPE_SAMPLED_TEXTURE:
			case DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
				srvuavcbvDescriptorCount += binding.arrayCount;
				bindingArrayCountMap[D3D12_DESCRIPTOR_RANGE_TYPE_SRV][binding.binding] = binding;
				break;
			case DESCRIPTOR_TYPE_STORAGE_TEXTURE:
			case DESCRIPTOR_TYPE_STORAGE_BUFFER:
				srvuavcbvDescriptorCount += binding.arrayCount;
				bindingArrayCountMap[D3D12_DESCRIPTOR_RANGE_TYPE_UAV][binding.binding] = binding;
				break;
			case DESCRIPTOR_TYPE_CONSTANT_BUFFER:
				srvuavcbvDescriptorCount += binding.arrayCount;
				bindingArrayCountMap[D3D12_DESCRIPTOR_RANGE_TYPE_CBV][binding.binding] = binding;
				break;
		}
	}

	uint32_t samplerDescCounter = 0, srvDescCounter = 0, uavDescCounter = 0, cbvDescCounter = 0, srvuavcbvDescCounter = 0;
	for (auto resourceTypeIt = bindingArrayCountMap.begin(); resourceTypeIt != bindingArrayCountMap.end(); resourceTypeIt++)
	{
		for (auto bindingArrayCountIt = resourceTypeIt->second.begin(); bindingArrayCountIt != resourceTypeIt->second.end(); bindingArrayCountIt++)
		{
			switch (bindingArrayCountIt->second.type)
			{
				case DESCRIPTOR_TYPE_SAMPLED_TEXTURE:
				case DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
					bindingRegisterMap[bindingArrayCountIt->second.binding] = srvDescCounter;
					srvDescCounter += bindingArrayCountIt->second.arrayCount;
					break;
				case DESCRIPTOR_TYPE_STORAGE_TEXTURE:
				case DESCRIPTOR_TYPE_STORAGE_BUFFER:
					bindingRegisterMap[bindingArrayCountIt->second.binding] = uavDescCounter;
					uavDescCounter += bindingArrayCountIt->second.arrayCount;
					break;
				case DESCRIPTOR_TYPE_CONSTANT_BUFFER:
					bindingRegisterMap[bindingArrayCountIt->second.binding] = cbvDescCounter;
					cbvDescCounter += bindingArrayCountIt->second.arrayCount;
					break;
				case DESCRIPTOR_TYPE_SAMPLER:
					bindingRegisterMap[bindingArrayCountIt->second.binding] = samplerDescCounter;
					bindingHeapOffsetMap[bindingArrayCountIt->second.binding] = samplerDescCounter;
					samplerDescCounter += bindingArrayCountIt->second.arrayCount;
					break;
			}

			if (bindingArrayCountIt->second.type != DESCRIPTOR_TYPE_SAMPLER)
			{
				bindingHeapOffsetMap[bindingArrayCountIt->second.binding] = srvuavcbvDescCounter;
				srvuavcbvDescCounter += bindingArrayCountIt->second.arrayCount;
			}
		}
	}
}

D3D12DescriptorPool::~D3D12DescriptorPool()
{
	std::vector<DescriptorSet> descSetsToFree;

	for (size_t i = 0; i < allocatedDescSets.size(); i++)
		descSetsToFree.push_back(static_cast<DescriptorSet>(allocatedDescSets[i]));

	freeDescriptorSets(descSetsToFree);
}

DescriptorSet D3D12DescriptorPool::allocateDescriptorSet()
{
	return allocateDescriptorSets(1)[0];
}

std::vector<DescriptorSet> D3D12DescriptorPool::allocateDescriptorSets(uint32_t setCount)
{
	std::vector<DescriptorSet> sets;

	for (uint32_t i = 0; i < setCount; i++)
	{
		D3D12DescriptorSet *descSet = new D3D12DescriptorSet();
		descSet->parentPool = this;
		descSet->srvUavCbvHeap = nullptr;
		descSet->srvUavCbvDescriptorCount = srvuavcbvDescriptorCount;
		descSet->samplerHeap = nullptr;
		descSet->samplerDescriptorCount = samplerDescriptorCount;
		descSet->nonStaticSamplerDescriptorCount = nonStaticSamplerDescriptorCount;
		descSet->bindingRegisterMap = bindingRegisterMap;
		descSet->bindingHeapOffsetMap = bindingHeapOffsetMap;

		if (samplerDescriptorCount > 0)
		{
			std::lock_guard<std::mutex> lockGuard(renderer->massDescriptorHeaps_mutex);

			int32_t massDescIndex = -1;
			uint32_t descBlockIndex = 0;

			uint32_t currentMassDescIndex = 0;
			for (uint32_t currentMassDescIndex = 0; massDescIndex == -1 && currentMassDescIndex < renderer->massDescriptorHeaps.size(); currentMassDescIndex++)
			{
				DescriptorHeap &descHeap = renderer->massDescriptorHeaps[currentMassDescIndex];

				if (descHeap.heap == nullptr)
				{

				}

				if (descHeap.heapType != D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
					continue;

				if (descHeap.numFreeDescriptors < samplerDescriptorCount)
					continue;

				uint32_t currentDescSlot = 0;
				uint32_t concurrrentFreeDescSlotCounter = 0;
				bool foundFreeDescRange = false;

				for (currentDescSlot = 0; currentDescSlot < descHeap.numDescriptors; currentDescSlot++)
				{
					if (descHeap.allocatedDescriptors[currentDescSlot] == 0)
						concurrrentFreeDescSlotCounter++;
					else
						concurrrentFreeDescSlotCounter = 0;

					if (concurrrentFreeDescSlotCounter >= samplerDescriptorCount)
					{
						foundFreeDescRange = true;
						break;
					}
				}

				if (foundFreeDescRange)
				{
					massDescIndex = currentMassDescIndex;
					descBlockIndex = currentDescSlot - samplerDescriptorCount + 1;

					break;
				}
			}

			if (massDescIndex != -1)
			{
				DescriptorHeap &descHeap = renderer->massDescriptorHeaps[massDescIndex];
				descHeap.numFreeDescriptors -= samplerDescriptorCount;

				for (uint32_t s = descBlockIndex; s < descBlockIndex + samplerDescriptorCount; s++)
					descHeap.allocatedDescriptors[s] = 1;

				descSet->samplerHeap = descHeap.heap;
				descSet->samplerMassHeapIndex = massDescIndex;
				descSet->samplerStartDescriptorSlot = descBlockIndex;
			}
			else
			{
				Log::get()->error("Couldn't find a free range of descriptor slots to allocate for a descriptor set");

				throw std::runtime_error("d3d12 error - no available descriptor slot ranges in heap pool");
			}
		}

		if (srvuavcbvDescriptorCount > 0)
		{
			std::lock_guard<std::mutex> lockGuard(renderer->massDescriptorHeaps_mutex);

			int32_t massDescIndex = -1;
			uint32_t descBlockIndex = 0;

			uint32_t currentMassDescIndex = 0;
			for (uint32_t currentMassDescIndex = 0; massDescIndex == -1 && currentMassDescIndex < renderer->massDescriptorHeaps.size(); currentMassDescIndex++)
			{
				DescriptorHeap &descHeap = renderer->massDescriptorHeaps[currentMassDescIndex];

				if (descHeap.heap == nullptr)
				{

				}

				if (descHeap.heapType != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
					continue;

				if (descHeap.numFreeDescriptors < srvuavcbvDescriptorCount)
					continue;

				uint32_t currentDescSlot = 0;
				uint32_t concurrrentFreeDescSlotCounter = 0;
				bool foundFreeDescRange = false;

				for (currentDescSlot = 0; currentDescSlot < descHeap.numDescriptors; currentDescSlot++)
				{
					if (descHeap.allocatedDescriptors[currentDescSlot] == 0)
						concurrrentFreeDescSlotCounter++;
					else
						concurrrentFreeDescSlotCounter = 0;

					if (concurrrentFreeDescSlotCounter >= srvuavcbvDescriptorCount)
					{
						foundFreeDescRange = true;
						break;
					}
				}

				if (foundFreeDescRange)
				{
					massDescIndex = currentMassDescIndex;
					descBlockIndex = currentDescSlot - srvuavcbvDescriptorCount + 1;

					break;
				}
			}

			if (massDescIndex != -1)
			{
				DescriptorHeap &descHeap = renderer->massDescriptorHeaps[massDescIndex];
				descHeap.numFreeDescriptors -= srvuavcbvDescriptorCount;
				
				for (uint32_t s = descBlockIndex; s < descBlockIndex + srvuavcbvDescriptorCount; s++)
					descHeap.allocatedDescriptors[s] = 1;

				descSet->srvUavCbvHeap = descHeap.heap;
				descSet->srvUavCbvMassHeapIndex = massDescIndex;
				descSet->srvUavCbvStartDescriptorSlot = descBlockIndex;
			}
			else
			{
				Log::get()->error("Couldn't find a free range of descriptor slots to allocate for a descriptor set");

				throw std::runtime_error("d3d12 error - no available descriptor slot ranges in heap pool");
			}
		}

		sets.push_back(descSet);
		allocatedDescSets.push_back(descSet);
	}

	return sets;
}

void D3D12DescriptorPool::freeDescriptorSet(DescriptorSet set)
{
	freeDescriptorSets({set});
}

void D3D12DescriptorPool::freeDescriptorSets(const std::vector<DescriptorSet> sets)
{
	for (size_t i = 0; i < sets.size(); i++)
	{
		D3D12DescriptorSet *set = static_cast<D3D12DescriptorSet*>(sets[i]);

		if (set->parentPool != this)
		{
			Log::get()->error("D3D12DescriptorPool: Cannot free a descriptor set from a pool that it wasn't allocated from");

			continue;
		}

		if (set->samplerDescriptorCount > 0)
		{
			std::lock_guard<std::mutex> lockGuard(renderer->massDescriptorHeaps_mutex);

			DescriptorHeap &descHeap = renderer->massDescriptorHeaps[set->samplerMassHeapIndex];
			descHeap.numFreeDescriptors += samplerDescriptorCount;
			
			for (uint32_t s = set->samplerStartDescriptorSlot; s < set->samplerStartDescriptorSlot + samplerDescriptorCount; s++)
				descHeap.allocatedDescriptors[s] = 0;
		}

		if (set->srvUavCbvDescriptorCount > 0)
		{
			std::lock_guard<std::mutex> lockGuard(renderer->massDescriptorHeaps_mutex);

			DescriptorHeap &descHeap = renderer->massDescriptorHeaps[set->srvUavCbvMassHeapIndex];
			descHeap.numFreeDescriptors += srvuavcbvDescriptorCount;

			for (uint32_t s = set->srvUavCbvStartDescriptorSlot; s < set->srvUavCbvStartDescriptorSlot + srvuavcbvDescriptorCount; s++)
				descHeap.allocatedDescriptors[s] = 0;
		}

		auto descSetIt = std::find(allocatedDescSets.begin(), allocatedDescSets.end(), set);

		if (descSetIt != allocatedDescSets.end())
			allocatedDescSets.erase(descSetIt);

		delete set;
	}
}

#endif