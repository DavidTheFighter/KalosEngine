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
#if BUILD_VULKAN_BACKEND
		return RENDERER_BACKEND_VULKAN;
#endif
	}
	
	if (std::find(launchArgs.begin(), launchArgs.end(), "-force_d3d12") != launchArgs.end())
	{
#if defined(_WIN32) && BUILD_D3D12_BACKEND
		return RENDERER_BACKEND_D3D12;
#endif
	}

#if BUILD_VULKAN_BACKEND
	return RENDERER_BACKEND_VULKAN;
#elif defined(_WIN32) && BUILD_D3D12_BACKEND
	return RENDERER_BACKEND_D3D12;
#else
	return RENDERER_BACKEND_MAX_ENUM;
#endif
}

Renderer* Renderer::allocateRenderer (const RendererAllocInfo& allocInfo)
{
	switch (allocInfo.backend)
	{
#if BUILD_VULKAN_BACKEND
		case RENDERER_BACKEND_VULKAN:
		{
			Log::get()->info("Allocating renderer w/ Vulkan backend");

			Renderer *renderer = new VulkanRenderer(allocInfo);

			return renderer;
		}
#endif
#if defined(_WIN32) && BUILD_D3D12_BACKEND
		case RENDERER_BACKEND_D3D12:
		{
			Log::get()->info("Allocating renderer w/ D3D12 backend\n");

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

RendererCommandPool::~RendererCommandPool()
{

}

RendererCommandBuffer::~RendererCommandBuffer()
{

}

RendererDescriptorPool::~RendererDescriptorPool()
{

}
