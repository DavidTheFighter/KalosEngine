#include "RendererCore/Vulkan/VulkanRenderGraph.h"
#if BUILD_VULKAN_BACKEND

#include <RendererCore/Vulkan/VulkanRenderer.h>

VulkanRenderGraph::VulkanRenderGraph(VulkanRenderer *rendererPtr)
{
	renderer = rendererPtr;

}

VulkanRenderGraph::~VulkanRenderGraph()
{
}

Semaphore VulkanRenderGraph::execute(bool returnWaitableSemaphore)
{
	return nullptr;
}

TextureView VulkanRenderGraph::getRenderGraphOutputTextureView()
{
	return nullptr;
}

void VulkanRenderGraph::assignPhysicalResources(const std::vector<size_t> &passStack)
{

}

void VulkanRenderGraph::finishBuild(const std::vector<size_t> &passStack)
{

}

#endif