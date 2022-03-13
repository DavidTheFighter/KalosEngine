#ifndef RENDERING_RENDERER_RENDERERDESCRIPTORPOOL_H_
#define RENDERING_RENDERER_RENDERERDESCRIPTORPOOL_H_

#include <common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class RendererDescriptorSet;

class RendererDescriptorPool
{
	public:

		virtual ~RendererDescriptorPool();

		virtual RendererDescriptorSet *allocateDescriptorSet () = 0;
		virtual std::vector<RendererDescriptorSet*> allocateDescriptorSets (uint32_t setCount) = 0;

		virtual void freeDescriptorSet (RendererDescriptorSet *set) = 0;
		virtual void freeDescriptorSets (const std::vector<RendererDescriptorSet*> sets) = 0;
};

typedef RendererDescriptorPool *DescriptorPool;

#endif /* RENDERING_RENDERER_RENDERERDESCRIPTORPOOL_H_ */
