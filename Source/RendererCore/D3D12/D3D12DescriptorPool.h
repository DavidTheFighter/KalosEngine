#ifndef RENDERING_D3D12_D3D12DESCRIPTORPOOL_H_
#define RENDERING_D3D12_D3D12DESCRIPTORPOOL_H_

#include <RendererCore/D3D12/d3d12_common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class D3D12DescriptorPool : public RendererDescriptorPool
{
	public:

	D3D12DescriptorPool();
	virtual ~D3D12DescriptorPool();

	DescriptorSet allocateDescriptorSet();
	std::vector<DescriptorSet> allocateDescriptorSets(uint32_t setCount);

	void freeDescriptorSet(DescriptorSet set);
	void freeDescriptorSets(const std::vector<DescriptorSet> sets);
};

#endif /* RENDERING_D3D12_D3D12DESCRIPTORPOOL_H_ */