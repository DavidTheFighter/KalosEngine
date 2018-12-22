#include "RendererCore/D3D12/D3D12DescriptorPool.h"

#include <RendererCore/D3D12/D3D12Objects.h>

D3D12DescriptorPool::D3D12DescriptorPool()
{

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
	std::vector<DescriptorSet> sets;

	for (uint32_t i = 0; i < setCount; i++)
	{
		D3D12DescriptorSet *descSet = new D3D12DescriptorSet();

		sets.push_back(descSet);
	}

	return sets;
}

void D3D12DescriptorPool::freeDescriptorSet(DescriptorSet set)
{

}

void D3D12DescriptorPool::freeDescriptorSets(const std::vector<DescriptorSet> sets)
{

}