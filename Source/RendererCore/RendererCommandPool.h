#ifndef RENDERING_RENDERER_RENDERERCOMMANDPOOL_H_
#define RENDERING_RENDERER_RENDERERCOMMANDPOOL_H_

#include <common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class RendererCommandBuffer;

class RendererCommandPool
{
	public:

		virtual ~RendererCommandPool();

		QueueType queue;
		CommandPoolFlags flags;

		virtual RendererCommandBuffer *allocateCommandBuffer (CommandBufferLevel level = COMMAND_BUFFER_LEVEL_PRIMARY) = 0;
		virtual std::vector<RendererCommandBuffer*> allocateCommandBuffers (CommandBufferLevel level, uint32_t commandBufferCount) = 0;

		virtual void resetCommandPoolAndFreeCommandBuffer (RendererCommandBuffer *commandBuffer) = 0;
		virtual void resetCommandPoolAndFreeCommandBuffers (const std::vector<RendererCommandBuffer*> &commandBuffers) = 0;

		virtual void resetCommandPool () = 0;
};

typedef RendererCommandPool *CommandPool;

#endif /* RENDERING_RENDERER_RENDERERCOMMANDPOOL_H_ */
