#ifndef RENDERING_D3D12_D3D12DESCRIPTORPOOL_H_
#define RENDERING_D3D12_D3D12DESCRIPTORPOOL_H_

#include <RendererCore/D3D12/d3d12_common.h>
#if BUILD_D3D12_BACKEND

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class D3D12Renderer;
class D3D12DescriptorSet;

class D3D12DescriptorPool : public RendererDescriptorPool
{
	public:

	D3D12DescriptorPool(D3D12Renderer *rendererPtr, const DescriptorSetLayoutDescription &descriptorSetLayout);
	virtual ~D3D12DescriptorPool();

	DescriptorSet allocateDescriptorSet();
	std::vector<DescriptorSet> allocateDescriptorSets(uint32_t setCount);

	void freeDescriptorSet(DescriptorSet set);
	void freeDescriptorSets(const std::vector<DescriptorSet> sets);

	private:

	D3D12Renderer *renderer;

	DescriptorSetLayoutDescription descriptorSetLayout;

	uint32_t srvuavcbvDescriptorCount;
	uint32_t samplerDescriptorCount;
	uint32_t nonStaticSamplerDescriptorCount;

	std::map<uint32_t, uint32_t> bindingRegisterMap;
	std::map<uint32_t, uint32_t> bindingHeapOffsetMap;
	std::vector<D3D12DescriptorSet*> allocatedDescSets;
};

#endif
#endif /* RENDERING_D3D12_D3D12DESCRIPTORPOOL_H_ */