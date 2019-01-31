#include "RendererCore/D3D12/D3D12DescriptorPool.h"
#if BUILD_D3D12_BACKEND

#include <RendererCore/D3D12/D3D12Renderer.h>

#include <RendererCore/D3D12/D3D12Objects.h>

D3D12DescriptorPool::D3D12DescriptorPool(D3D12Renderer *rendererPtr, const DescriptorSetLayoutDescription &descriptorSetLayout)
{
	renderer = rendererPtr;
	this->descriptorSetLayout = descriptorSetLayout;
}

D3D12DescriptorPool::~D3D12DescriptorPool()
{

}

DescriptorSet D3D12DescriptorPool::allocateDescriptorSet()
{
	return allocateDescriptorSets(1)[0];
}

std::vector<DescriptorSet> D3D12DescriptorPool::allocateDescriptorSets(uint32_t setCount)
{
	uint32_t srvuavcbvDescCount = descriptorSetLayout.constantBufferDescriptorCount + descriptorSetLayout.inputAttachmentDescriptorCount + descriptorSetLayout.sampledTextureDescriptorCount;
	uint32_t samplerDescCount = descriptorSetLayout.samplerDescriptorCount;

	std::vector<DescriptorSet> sets;

	for (uint32_t i = 0; i < setCount; i++)
	{
		D3D12DescriptorSet *descSet = new D3D12DescriptorSet();
		descSet->parentPool = this;
		descSet->srvUavCbvHeap = nullptr;
		descSet->srvUavCbvDescriptorCount = srvuavcbvDescCount;
		descSet->samplerHeap = nullptr;
		descSet->samplerDescriptorCount = samplerDescCount;
		descSet->samplerCount = descriptorSetLayout.samplerDescriptorCount;
		descSet->constantBufferCount = descriptorSetLayout.constantBufferDescriptorCount;
		descSet->inputAtttachmentCount = descriptorSetLayout.inputAttachmentDescriptorCount;
		descSet->sampledTextureCount = descriptorSetLayout.sampledTextureDescriptorCount;

		if (samplerDescCount > 0)
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

				if (descHeap.numFreeDescriptors < samplerDescCount)
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

					if (concurrrentFreeDescSlotCounter >= samplerDescCount)
					{
						foundFreeDescRange = true;
						break;
					}
				}

				if (foundFreeDescRange)
				{
					massDescIndex = currentMassDescIndex;
					descBlockIndex = currentDescSlot - samplerDescCount + 1;

					break;
				}
			}

			if (massDescIndex != -1)
			{
				DescriptorHeap &descHeap = renderer->massDescriptorHeaps[massDescIndex];
				descHeap.numFreeDescriptors -= samplerDescCount;

				for (uint32_t s = descBlockIndex; s < descBlockIndex + samplerDescCount; s++)
					descHeap.allocatedDescriptors[s] = 0;

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

		if (srvuavcbvDescCount > 0)
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

				if (descHeap.numFreeDescriptors < srvuavcbvDescCount)
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

					if (concurrrentFreeDescSlotCounter >= srvuavcbvDescCount)
					{
						foundFreeDescRange = true;
						break;
					}
				}

				if (foundFreeDescRange)
				{
					massDescIndex = currentMassDescIndex;
					descBlockIndex = currentDescSlot - srvuavcbvDescCount + 1;

					break;
				}
			}

			if (massDescIndex != -1)
			{
				DescriptorHeap &descHeap = renderer->massDescriptorHeaps[massDescIndex];
				descHeap.numFreeDescriptors -= srvuavcbvDescCount;
				
				for (uint32_t s = descBlockIndex; s < descBlockIndex + srvuavcbvDescCount; s++)
					descHeap.allocatedDescriptors[s] = 0;

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
	}

	return sets;
}

void D3D12DescriptorPool::freeDescriptorSet(DescriptorSet set)
{
	freeDescriptorSets({set});
}

void D3D12DescriptorPool::freeDescriptorSets(const std::vector<DescriptorSet> sets)
{
	uint32_t samplerDescCount = descriptorSetLayout.samplerDescriptorCount;
	uint32_t srvuavcbvDescCount = descriptorSetLayout.constantBufferDescriptorCount + descriptorSetLayout.inputAttachmentDescriptorCount + descriptorSetLayout.sampledTextureDescriptorCount;

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
			descHeap.numFreeDescriptors += samplerDescCount;
			
			for (uint32_t s = set->samplerStartDescriptorSlot; s < set->samplerStartDescriptorSlot + samplerDescCount; s++)
				descHeap.allocatedDescriptors[s] = 0;
		}

		if (set->srvUavCbvDescriptorCount > 0)
		{
			std::lock_guard<std::mutex> lockGuard(renderer->massDescriptorHeaps_mutex);

			DescriptorHeap &descHeap = renderer->massDescriptorHeaps[set->srvUavCbvMassHeapIndex];
			descHeap.numFreeDescriptors += srvuavcbvDescCount;

			for (uint32_t s = set->srvUavCbvStartDescriptorSlot; s < set->srvUavCbvStartDescriptorSlot + srvuavcbvDescCount; s++)
				descHeap.allocatedDescriptors[s] = 0;
		}

		delete set;
	}
}

#endif