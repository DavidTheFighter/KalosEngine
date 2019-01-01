#include "RendererCore/Renderer.h"

#include <RendererCore/Vulkan/VulkanRenderer.h>
#include <RendererCore/D3D12/D3D12Renderer.h>

Renderer::Renderer()
{
	
}

Renderer::~Renderer()
{

}

/*
 * Determines a renderer backend based on the platform and launch args.
 */
RendererBackend Renderer::chooseRendererBackend (const std::vector<std::string>& launchArgs)
{
	// Check if the commmand line args forced an api first
	if (std::find(launchArgs.begin(), launchArgs.end(), "-force_vulkan") != launchArgs.end())
	{
		return RENDERER_BACKEND_VULKAN;
	}
	else if (std::find(launchArgs.begin(), launchArgs.end(), "-force_d3d12") != launchArgs.end())
	{
#ifdef _WIN32
		return RENDERER_BACKEND_D3D12;
#endif
	}

	// Always use vulkan by default
	return RENDERER_BACKEND_VULKAN;
}

Renderer* Renderer::allocateRenderer (const RendererAllocInfo& allocInfo)
{
	switch (allocInfo.backend)
	{
		case RENDERER_BACKEND_VULKAN:
		{
			Log::get()->info("Allocating renderer w/ Vulkan backend");

			Renderer *renderer = new VulkanRenderer(allocInfo);

			return renderer;
		}
#ifdef _WIN32
		case RENDERER_BACKEND_D3D12:
		{
			Log::get()->info("Allocating rednerer w/ D3D12 backend\n");

			Renderer *renderer = new D3D12Renderer(allocInfo);

			return renderer;
		}
#endif
		default:
		{
			Log::get()->error("Failed to allocate a renderer w/ unknown backend type: {}", allocInfo.backend);

			return nullptr;
		}
	}
}

CommandBuffer Renderer::beginSingleTimeCommand (CommandPool pool)
{
	CommandBuffer cmdBuffer = pool->allocateCommandBuffer(COMMAND_BUFFER_LEVEL_PRIMARY);

	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	return cmdBuffer;
}

void Renderer::endSingleTimeCommand (CommandBuffer cmdBuffer, CommandPool pool, QueueType queue)
{
	cmdBuffer->endCommands();

	submitToQueue(queue, {cmdBuffer});
	waitForQueueIdle(queue);

	pool->freeCommandBuffer(cmdBuffer);
}

RendererCommandPool::~RendererCommandPool()
{

}

RendererCommandBuffer::~RendererCommandBuffer()
{

}

RendererDescriptorPool::~RendererDescriptorPool()
{

}
