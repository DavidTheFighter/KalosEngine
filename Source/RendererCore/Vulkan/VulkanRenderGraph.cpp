#include "RendererCore/Vulkan/VulkanRenderGraph.h"
#if BUILD_VULKAN_BACKEND

#include <RendererCore/Vulkan/VulkanRenderer.h>

VulkanRenderGraph::VulkanRenderGraph(VulkanRenderer *rendererPtr)
{
	renderer = rendererPtr;

	execCounter = 0;

	enableRenderPassMerging = true;
	enableResourceAliasing = true;

	for (size_t i = 0; i < renderer->getMaxFramesInFlight(); i++)
		gfxCommandPools.push_back(renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT | COMMAND_POOL_RESET_COMMAND_BUFFER_BIT));

	for (size_t i = 0; i < renderer->getMaxFramesInFlight(); i++)
		executionDoneSemaphores.push_back(renderer->createSemaphore());

	for (size_t i = 0; i < renderer->getMaxFramesInFlight(); i++)
		gfxCommandBuffers.push_back(gfxCommandPools[i]->allocateCommandBuffer());
}

VulkanRenderGraph::~VulkanRenderGraph()
{
	for (CommandPool pool : gfxCommandPools)
		renderer->destroyCommandPool(pool);

	for (Semaphore sem : executionDoneSemaphores)
		renderer->destroySemaphore(sem);

	for (VulkanRenderGraphImage image : graphTextures)
		renderer->destroyTexture(image.rendererTexture);

	for (size_t i = 0; i < finalRenderPasses.size(); i++)
	{
		vkDestroyRenderPass(renderer->device, finalRenderPasses[i].renderPassHandle, nullptr);
		vkDestroyFramebuffer(renderer->device, finalRenderPasses[i].framebufferHandle, nullptr);
	}

	for (auto viewIt = graphTextureViews.begin(); viewIt != graphTextureViews.end(); viewIt++)
		renderer->destroyTextureView(viewIt->second.textureView);

	for (auto bufferIt = graphBuffers.begin(); bufferIt != graphBuffers.end(); bufferIt++)
		renderer->destroyBuffer(bufferIt->second);
}

Semaphore VulkanRenderGraph::execute(bool returnWaitableSemaphore)
{
	VulkanCommandBuffer *cmdBuffer = static_cast<VulkanCommandBuffer*>(gfxCommandBuffers[execCounter % gfxCommandBuffers.size()]);
	gfxCommandPools[execCounter % gfxCommandPools.size()]->resetCommandPool();

	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	for (size_t pass = 0; pass < finalRenderPasses.size(); pass++)
	{
		const VulkanRenderGraphRenderPass &passData = finalRenderPasses[pass];

		if ((passData.beforeRenderImageBarriers.size() + passData.beforeRenderBufferBarriers.size() + passData.beforeRenderMemoryBarriers.size()) > 0)
			vkCmdPipelineBarrier(cmdBuffer->bufferHandle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, (uint32_t) passData.beforeRenderMemoryBarriers.size(), passData.beforeRenderMemoryBarriers.data(), (uint32_t) passData.beforeRenderBufferBarriers.size(), passData.beforeRenderBufferBarriers.data(), (uint32_t) passData.beforeRenderImageBarriers.size(), passData.beforeRenderImageBarriers.data());

		if (passData.pipelineType == RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS)
		{
			cmdBuffer->vulkan_beginRenderPass(passData.renderPassHandle, passData.framebufferHandle, {0, 0, passData.renderArea.x, passData.renderArea.y}, passData.clearValues, passData.subpassContents);
			cmdBuffer->setScissors(0, {{0, 0, passData.renderArea.x, passData.renderArea.y}});
			cmdBuffer->setViewports(0, {{0.0f, 0.0f, (float) passData.renderArea.x, (float) passData.renderArea.y, 0.0f, 1.0f}});
		}

		for (size_t i = 0; i < passData.passIndices.size(); i++)
		{
			RenderGraphRenderFunctionData renderData = {};

			cmdBuffer->beginDebugRegion(passes[passData.passIndices[i]]->getPassName(), glm::vec4(0.0f, 0.5f, 1.0f, 1.0f));

			if (passes[passData.passIndices[i]]->hasRenderFunction())
				passes[passData.passIndices[i]]->getRenderFunction()(cmdBuffer, renderData);

			cmdBuffer->endDebugRegion();
		}

		if (passData.pipelineType == RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS)
			cmdBuffer->vulkan_endRenderPass();

		if ((passData.afterRenderImageBarriers.size() + passData.afterRenderBufferBarriers.size() + passData.afterRenderMemoryBarriers.size()) > 0)
			vkCmdPipelineBarrier(cmdBuffer->bufferHandle, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, (uint32_t) passData.afterRenderMemoryBarriers.size(), passData.afterRenderMemoryBarriers.data(), (uint32_t) passData.afterRenderBufferBarriers.size(), passData.afterRenderBufferBarriers.data(), (uint32_t) passData.afterRenderImageBarriers.size(), passData.afterRenderImageBarriers.data());
	}

	cmdBuffer->endCommands();

	std::vector<Semaphore> waitSems = {};
	std::vector<PipelineStageFlags> waitSemStages = {};
	std::vector<Semaphore> signalSems = {};

	if (returnWaitableSemaphore)
		signalSems.push_back(executionDoneSemaphores[execCounter % executionDoneSemaphores.size()]);

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, waitSems, waitSemStages, signalSems, nullptr);

	execCounter++;

	return returnWaitableSemaphore ? signalSems[0] : nullptr;
}

void VulkanRenderGraph::resizeNamedSize(const std::string &sizeName, glm::uvec2 newSize)
{
	namedSizes[sizeName] = newSize;

	std::vector<VulkanTexture*> resizedTextures;

	for (size_t i = 0; i < graphTextures.size(); i++)
	{
		if (graphTextures[i].attachment.namedRelativeSize == sizeName)
		{
			VulkanRenderGraphImage &graphImage = graphTextures[i];

			resizedTextures.push_back(graphImage.rendererTexture);

			vmaDestroyImage(renderer->memAllocator, graphImage.rendererTexture->imageHandle, graphImage.rendererTexture->imageMemory);
			//graphImage.rendererTexture = static_cast<VulkanTexture*>(renderer->createTexture({tempGraphImageCopy.width, tempGraphImageCopy.height, tempGraphImageCopy.depth}, tempGraphImageCopy.textureFormat, tempGraphImageCopy.usage, MEMORY_USAGE_GPU_ONLY, true, tempGraphImageCopy.mipCount, tempGraphImageCopy.layerCount, 1));
		
			uint32_t sizeX = graphImage.attachment.namedRelativeSize == "" ? uint32_t(graphImage.attachment.sizeX) : uint32_t(namedSizes[graphImage.attachment.namedRelativeSize].x * graphImage.attachment.sizeX);
			uint32_t sizeY = graphImage.attachment.namedRelativeSize == "" ? uint32_t(graphImage.attachment.sizeY) : uint32_t(namedSizes[graphImage.attachment.namedRelativeSize].y * graphImage.attachment.sizeY);

			VkImageCreateInfo imageCreateInfo = {};
			imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			imageCreateInfo.extent = {sizeX, sizeY, 1};
			imageCreateInfo.imageType = sizeY > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D;
			imageCreateInfo.mipLevels = graphImage.attachment.mipLevels;
			imageCreateInfo.arrayLayers = graphImage.attachment.arrayLayers;
			imageCreateInfo.format = ResourceFormatToVkFormat(graphImage.attachment.format);
			imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
			imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			imageCreateInfo.usage = graphImage.rendererTexture->usage;
			imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			imageCreateInfo.flags = (graphImage.attachment.arrayLayers == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0);

			switch (graphImage.attachment.samples)
			{
				case 1:
					imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
					break;
				case 2:
					imageCreateInfo.samples = VK_SAMPLE_COUNT_2_BIT;
					break;
				case 4:
					imageCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
					break;
				case 8:
					imageCreateInfo.samples = VK_SAMPLE_COUNT_8_BIT;
					break;
				case 16:
					imageCreateInfo.samples = VK_SAMPLE_COUNT_16_BIT;
					break;
				default:
					imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
			}

			VmaAllocationCreateInfo allocInfo = {};
			allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
			allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

			VulkanTexture *vulkanTexture = graphImage.rendererTexture;
			vulkanTexture->width = sizeX;
			vulkanTexture->height = sizeY;
			vulkanTexture->depth = 1;
			vulkanTexture->textureFormat = graphImage.attachment.format;
			vulkanTexture->usage = graphImage.rendererTexture->usage;
			vulkanTexture->layerCount = graphImage.attachment.arrayLayers;
			vulkanTexture->mipCount = graphImage.attachment.mipLevels;

			VK_CHECK_RESULT(vmaCreateImage(renderer->memAllocator, &imageCreateInfo, &allocInfo, &vulkanTexture->imageHandle, &vulkanTexture->imageMemory, nullptr));

			renderer->setObjectDebugName(vulkanTexture, OBJECT_TYPE_TEXTURE, graphImage.debugName);
		}
	}

	for (auto textureViewIt = graphTextureViews.begin(); textureViewIt != graphTextureViews.end(); textureViewIt++)
	{
		auto resizedTextureIt = std::find(resizedTextures.begin(), resizedTextures.end(), static_cast<VulkanTexture*>(textureViewIt->second.textureView->parentTexture));

		if (resizedTextureIt != resizedTextures.end())
		{
			VulkanTextureView tempTextureViewCopy = *static_cast<VulkanTextureView*>(textureViewIt->second.textureView);

			renderer->destroyTextureView(textureViewIt->second.textureView);
			textureViewIt->second.textureView = renderer->createTextureView((*resizedTextureIt), tempTextureViewCopy.viewType, {tempTextureViewCopy.baseMip, tempTextureViewCopy.mipCount, tempTextureViewCopy.baseLayer, tempTextureViewCopy.layerCount}, tempTextureViewCopy.viewFormat);
		}
	}

	for (VulkanRenderGraphRenderPass &pass : finalRenderPasses)
	{
		bool foundResizedTextureView = false;

		for (const std::string &textureViewName : pass.framebufferTextureViews)
		{
			if (graphTextureViews[textureViewName].attachment.namedRelativeSize == sizeName)
			{
				foundResizedTextureView = true;
				break;
			}
		}

		if (foundResizedTextureView)
		{
			vkDestroyFramebuffer(renderer->device, pass.framebufferHandle, nullptr);

			std::vector<VkImageView> framebufferVkImageViews;

			for (const std::string &textureViewName : pass.framebufferTextureViews)
				framebufferVkImageViews.push_back(static_cast<VulkanTextureView *>(graphTextureViews[textureViewName].textureView)->imageView);

			pass.renderArea = glm::uvec3(newSize.x, newSize.y, pass.renderArea.z);

			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.pNext = nullptr;
			framebufferCreateInfo.flags = 0;
			framebufferCreateInfo.renderPass = pass.renderPassHandle;
			framebufferCreateInfo.attachmentCount = (uint32_t)framebufferVkImageViews.size();
			framebufferCreateInfo.pAttachments = framebufferVkImageViews.data();
			framebufferCreateInfo.width = pass.renderArea.x;
			framebufferCreateInfo.height = pass.renderArea.y;
			framebufferCreateInfo.layers = pass.renderArea.z;

			VK_CHECK_RESULT(vkCreateFramebuffer(renderer->device, &framebufferCreateInfo, nullptr, &pass.framebufferHandle));
		}
	}
}

const std::string viewSelectMipPostfix = "_fg_miplvl";
const std::string viewSelectLayerPostfix = "_fg_layer";

void VulkanRenderGraph::assignPhysicalResources(const std::vector<size_t> &passStack)
{
	typedef struct
	{
		RenderPassAttachment attachment;
		VkImageUsageFlags usageFlags;

	} PhysicalTextureData;

	typedef struct
	{
		uint32_t size;
		VkBufferUsageFlags usageFlags;
	} PhysicalBufferData;

	std::map<std::string, PhysicalTextureData> textureData;
	std::map<std::string, PhysicalBufferData> bufferData;

	// Gather information about each resource and how it will be used
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RenderGraphRenderPass &pass = *passes[passStack[i]];

		for (size_t i = 0; i < pass.getSampledTextureInputs().size(); i++)
			textureData[pass.getSampledTextureInputs()[i]].usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

		for (size_t i = 0; i < pass.getInputAttachmentInputs().size(); i++)
			textureData[pass.getInputAttachmentInputs()[i]].usageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
		{
			PhysicalTextureData data = {};
			data.attachment = pass.getColorAttachmentOutputs()[o].attachment;
			data.usageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			if (pass.getColorAttachmentOutputs()[o].textureName == outputAttachmentName)
				data.usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

			const std::string &textureName = pass.getColorAttachmentOutputs()[o].textureName;

			textureData[textureName] = data;
			graphImageViewsInitialImageLayout[textureName] = VK_IMAGE_LAYOUT_UNDEFINED;
		}

		if (pass.hasDepthStencilOutput())
		{
			PhysicalTextureData data = {};
			data.attachment = pass.getDepthStencilAttachmentOutput().attachment;
			data.usageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			if (pass.getDepthStencilAttachmentOutput().textureName == outputAttachmentName)
				data.usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

			const std::string &textureName = pass.getDepthStencilAttachmentOutput().textureName;

			textureData[textureName] = data;
			graphImageViewsInitialImageLayout[textureName] = VK_IMAGE_LAYOUT_UNDEFINED;
		}

		for (size_t o = 0; o < pass.getStorageTextures().size(); o++)
		{
			const std::string &textureName = pass.getStorageTextures()[o].textureName;

			if (pass.getStorageTextures()[o].canWriteAsOutput)
			{
				PhysicalTextureData data = {};
				data.attachment = pass.getStorageTextures()[o].attachment;
				data.usageFlags = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

				if (pass.getStorageTextures()[o].textureName == outputAttachmentName)
					data.usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;

				textureData[textureName] = data;
				graphImageViewsInitialImageLayout[textureName] = VK_IMAGE_LAYOUT_UNDEFINED;
			}
			else
			{
				PhysicalTextureData &data = textureData[textureName];
				data.usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			}
		}

		for (size_t sb = 0; sb < pass.getStorageBuffers().size(); sb++)
		{
			const std::string &bufferName = pass.getStorageBuffers()[sb].bufferName;

			if (pass.getStorageBuffers()[sb].canWriteAsOutput)
			{
				PhysicalBufferData data = {};
				data.size = pass.getStorageBuffers()[sb].size;
				data.usageFlags = BufferUsageFlagsToVkBufferUsageFlags(pass.getStorageBuffers()[sb].usage);
				
				bufferData[bufferName] = data;
			}
		}
	}

	// Actually create all the resources
	for (auto it = textureData.begin(); it != textureData.end(); it++)
	{
		const PhysicalTextureData &data = it->second;

		uint32_t sizeX = data.attachment.namedRelativeSize == "" ? uint32_t(data.attachment.sizeX) : uint32_t(namedSizes[data.attachment.namedRelativeSize].x * data.attachment.sizeX);
		uint32_t sizeY = data.attachment.namedRelativeSize == "" ? uint32_t(data.attachment.sizeY) : uint32_t(namedSizes[data.attachment.namedRelativeSize].y * data.attachment.sizeY);

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.extent = {sizeX, sizeY, 1};
		imageCreateInfo.imageType = sizeY > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D;
		imageCreateInfo.mipLevels = data.attachment.mipLevels;
		imageCreateInfo.arrayLayers = data.attachment.arrayLayers;
		imageCreateInfo.format = ResourceFormatToVkFormat(data.attachment.format);
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = data.usageFlags;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.flags = (data.attachment.arrayLayers == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0);

		switch (data.attachment.samples)
		{
			case 1:
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
				break;
			case 2:
				imageCreateInfo.samples = VK_SAMPLE_COUNT_2_BIT;
				break;
			case 4:
				imageCreateInfo.samples = VK_SAMPLE_COUNT_4_BIT;
				break;
			case 8:
				imageCreateInfo.samples = VK_SAMPLE_COUNT_8_BIT;
				break;
			case 16:
				imageCreateInfo.samples = VK_SAMPLE_COUNT_16_BIT;
				break;
			default:
				imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		}

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

		VulkanTexture *vulkanTexture = new VulkanTexture();
		vulkanTexture->width = sizeX;
		vulkanTexture->height = sizeY;
		vulkanTexture->depth = 1;
		vulkanTexture->textureFormat = data.attachment.format;
		vulkanTexture->usage = data.usageFlags;
		vulkanTexture->layerCount = data.attachment.arrayLayers;
		vulkanTexture->mipCount = data.attachment.mipLevels;

		VK_CHECK_RESULT(vmaCreateImage(renderer->memAllocator, &imageCreateInfo, &allocInfo, &vulkanTexture->imageHandle, &vulkanTexture->imageMemory, nullptr));

		renderer->setObjectDebugName(vulkanTexture, OBJECT_TYPE_TEXTURE, it->first);

		VulkanRenderGraphImage graphImage = {};
		graphImage.attachment = data.attachment;
		graphImage.rendererTexture = vulkanTexture;
		graphImage.debugName = it->first;

		graphTextures.push_back(graphImage);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = vulkanTexture->imageHandle;
		imageViewCreateInfo.viewType = toVkImageViewType(data.attachment.viewType);
		imageViewCreateInfo.format = imageCreateInfo.format;
		imageViewCreateInfo.subresourceRange.aspectMask = isVkDepthFormat(imageViewCreateInfo.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = data.attachment.mipLevels;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = data.attachment.arrayLayers;

		VulkanTextureView *vulkanTextureView = new VulkanTextureView();
		vulkanTextureView->parentTexture = vulkanTexture;
		vulkanTextureView->viewType = data.attachment.viewType;
		vulkanTextureView->viewFormat = data.attachment.format;
		vulkanTextureView->baseMip = 0;
		vulkanTextureView->mipCount = data.attachment.mipLevels;
		vulkanTextureView->baseLayer = 0;
		vulkanTextureView->layerCount = data.attachment.arrayLayers;

		VK_CHECK_RESULT(vkCreateImageView(renderer->device, &imageViewCreateInfo, nullptr, &vulkanTextureView->imageView));

		renderer->setObjectDebugName(vulkanTextureView, OBJECT_TYPE_TEXTURE_VIEW, it->first);

		VulkanRenderGraphTextureView graphTextureView = {};
		graphTextureView.attachment = data.attachment;
		graphTextureView.textureView = vulkanTextureView;

		graphTextureViews[it->first] = graphTextureView;

		imageViewCreateInfo.subresourceRange.layerCount = 1;
		imageViewCreateInfo.subresourceRange.levelCount = 1;

		for (uint32_t m = 0; m < data.attachment.mipLevels; m++)
		{
			vulkanTextureView = new VulkanTextureView();
			vulkanTextureView->parentTexture = vulkanTexture;
			vulkanTextureView->viewType = data.attachment.viewType;
			vulkanTextureView->viewFormat = data.attachment.format;
			vulkanTextureView->baseMip = m;
			vulkanTextureView->mipCount = 1;
			vulkanTextureView->baseLayer = 0;
			vulkanTextureView->layerCount = data.attachment.arrayLayers;

			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.baseMipLevel = m;

			VK_CHECK_RESULT(vkCreateImageView(renderer->device, &imageViewCreateInfo, nullptr, &vulkanTextureView->imageView));

			renderer->setObjectDebugName(vulkanTextureView, OBJECT_TYPE_TEXTURE_VIEW, it->first + viewSelectMipPostfix + toString(m));

			graphTextureView.textureView = vulkanTextureView;
			graphTextureViews[it->first + viewSelectMipPostfix + toString(m)] = graphTextureView;
		}

		for (uint32_t l = 0; l < data.attachment.arrayLayers; l++)
		{
			vulkanTextureView = new VulkanTextureView();
			vulkanTextureView->parentTexture = vulkanTexture;
			vulkanTextureView->viewType = data.attachment.viewType;
			vulkanTextureView->viewFormat = data.attachment.format;
			vulkanTextureView->baseMip = 0;
			vulkanTextureView->mipCount = 1;
			vulkanTextureView->baseLayer = l;
			vulkanTextureView->layerCount = 1;

			imageViewCreateInfo.subresourceRange.baseArrayLayer = l;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;

			VK_CHECK_RESULT(vkCreateImageView(renderer->device, &imageViewCreateInfo, nullptr, &vulkanTextureView->imageView));

			renderer->setObjectDebugName(vulkanTextureView, OBJECT_TYPE_TEXTURE_VIEW, it->first + viewSelectLayerPostfix + toString(l));

			graphTextureView.textureView = vulkanTextureView;
			graphTextureViews[it->first + viewSelectLayerPostfix + toString(l)] = graphTextureView;
		}
	}

	for (auto it = bufferData.begin(); it != bufferData.end(); it++)
	{
		const PhysicalBufferData &data = it->second;

		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.pNext = nullptr;
		bufferInfo.flags = 0;
		bufferInfo.size = data.size;
		bufferInfo.usage = data.usageFlags;
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		bufferInfo.queueFamilyIndexCount = 0;
		bufferInfo.pQueueFamilyIndices = nullptr;

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.flags = 0;

		VulkanBuffer *buffer = new VulkanBuffer();
		buffer->bufferSize = data.size;
		buffer->memorySize = data.size;

		VK_CHECK_RESULT(vmaCreateBuffer(renderer->memAllocator, &bufferInfo, &allocInfo, &buffer->bufferHandle, &buffer->bufferMemory, nullptr));

		renderer->setObjectDebugName(buffer, OBJECT_TYPE_BUFFER, it->first);

		graphBuffers[it->first] = buffer;
	}
}

void VulkanRenderGraph::finishBuild(const std::vector<size_t> &passStack)
{
	typedef struct
	{
		VkImageLayout currentLayout;

	} RenderPassTextureState;

	typedef struct
	{
		BufferLayout currentLayout;
	} RenderPassBufferState;

	std::map<std::string, RenderPassTextureState> textureStates;
	std::map<std::string, RenderPassBufferState> bufferStates;

	/*
	Initialize all resources with the last state they would be in after a full render graph execution,
	as this will be the first state we transition out of when we execute it again. For the vulkan backend
	most states will be UNDEFINED, as we discard almost all data (besides any kind of temporal reads).
	*/
	for (auto initialResourceStatesIt = graphImageViewsInitialImageLayout.begin(); initialResourceStatesIt != graphImageViewsInitialImageLayout.end(); initialResourceStatesIt++)
	{
		RenderPassTextureState state = {};
		state.currentLayout = initialResourceStatesIt->second;

		textureStates[initialResourceStatesIt->first] = state;
	}

	for (auto bufferIt = graphBuffers.begin(); bufferIt != graphBuffers.end(); bufferIt++)
	{
		RenderPassBufferState state = {};
		state.currentLayout = BUFFER_LAYOUT_MAX_ENUM;

		bufferStates[bufferIt->first] = state;
	}

	for (size_t passStackIndex = 0; passStackIndex < passStack.size(); passStackIndex++)
	{
		VulkanRenderGraphRenderPass passData = {};
		passData.pipelineType = passes[passStack[passStackIndex]]->getPipelineType();

		if (passData.pipelineType == RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS)
		{
			size_t basePassStackIndex = passStackIndex;

			// Find out how many passes we can merge together (they should already be sorted so we can just step thru them)
			while (enableRenderPassMerging && passStackIndex < passStack.size() - 1 && checkIsMergeValid(passStack[passStackIndex + 1], passStack[passStackIndex], passStack))
				passStackIndex++;

			size_t endPassStackIndex = passStackIndex;
			std::vector<VkAttachmentDescription> passAttachments;
			std::vector<VkSubpassDescription> passSubpasses;
			std::vector<VkSubpassDependency> passDependencies;
			std::vector<std::string> passAttachmentImageViews;

			std::vector<std::vector<VkAttachmentReference>> subpassAttachmentRefs(endPassStackIndex - basePassStackIndex + 1); // One vector<..> per subpass, so that the memory pointers stay valid
			std::vector<std::vector<uint32_t>> subpassPreserveAttachments(endPassStackIndex - basePassStackIndex + 1); // One vector<..> per subpass, so that the memory pointers stay valid

			std::map<std::string, size_t> imageViewPassAttachmentsIndex; // Maps an image view/texture name to it's index of VkAttachmentDescriptions

			const RenderPassOutputAttachment &outputAttachmentExample = passes[passStack[basePassStackIndex]]->hasDepthStencilOutput() ? passes[passStack[basePassStackIndex]]->getDepthStencilAttachmentOutput() : passes[passStack[basePassStackIndex]]->getColorAttachmentOutputs()[0];
			uint32_t sizeX = outputAttachmentExample.attachment.namedRelativeSize == "" ? uint32_t(outputAttachmentExample.attachment.sizeX) : uint32_t(namedSizes[outputAttachmentExample.attachment.namedRelativeSize].x * outputAttachmentExample.attachment.sizeX);
			uint32_t sizeY = outputAttachmentExample.attachment.namedRelativeSize == "" ? uint32_t(outputAttachmentExample.attachment.sizeY) : uint32_t(namedSizes[outputAttachmentExample.attachment.namedRelativeSize].y * outputAttachmentExample.attachment.sizeY);

			passData.renderArea = {sizeX, sizeY, outputAttachmentExample.attachment.arrayLayers};
			passData.subpassContents = VK_SUBPASS_CONTENTS_INLINE;

			// Setup the attachment descriptions for the output attachments
			for (size_t p = basePassStackIndex; p <= passStackIndex; p++)
			{
				const RenderGraphRenderPass &pass = *passes[passStack[p]];

				passData.passIndices.push_back(passStack[p]);

				for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
				{
					const RenderPassOutputAttachment &outputAttachment = pass.getColorAttachmentOutputs()[o];
					VkAttachmentDescription attachmentDesc = {};
					attachmentDesc.flags = 0;
					attachmentDesc.format = ResourceFormatToVkFormat(outputAttachment.attachment.format);
					attachmentDesc.samples = sampleCountToVkSampleCount(outputAttachment.attachment.samples);
					attachmentDesc.loadOp = outputAttachment.clearAttachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachmentDesc.storeOp = endPassStackIndex >= attachmentUsageLifetimes[outputAttachment.textureName].y ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
					attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					passAttachments.push_back(attachmentDesc);
					passAttachmentImageViews.push_back(outputAttachment.textureName);
					passData.clearValues.push_back(*reinterpret_cast<const VkClearValue*>(&outputAttachment.attachmentClearValue));
					imageViewPassAttachmentsIndex[outputAttachment.textureName] = passAttachments.size() - 1;
					textureStates[outputAttachment.textureName].currentLayout = attachmentDesc.finalLayout;
				}

				if (pass.hasDepthStencilOutput())
				{
					const RenderPassOutputAttachment &outputAttachment = pass.getDepthStencilAttachmentOutput();
					VkAttachmentDescription attachmentDesc = {};
					attachmentDesc.flags = 0;
					attachmentDesc.format = ResourceFormatToVkFormat(outputAttachment.attachment.format);
					attachmentDesc.samples = sampleCountToVkSampleCount(outputAttachment.attachment.samples);;
					attachmentDesc.loadOp = outputAttachment.clearAttachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachmentDesc.storeOp = endPassStackIndex >= attachmentUsageLifetimes[outputAttachment.textureName].y ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
					attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
					attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

					passAttachments.push_back(attachmentDesc);
					passAttachmentImageViews.push_back(outputAttachment.textureName);
					passData.clearValues.push_back(*reinterpret_cast<const VkClearValue*>(&outputAttachment.attachmentClearValue));
					imageViewPassAttachmentsIndex[outputAttachment.textureName] = passAttachments.size() - 1;
					textureStates[outputAttachment.textureName].currentLayout = attachmentDesc.finalLayout;
				}
			}

			// Setup the attachment descriptions for the input attachments
			for (size_t p = basePassStackIndex; p <= passStackIndex; p++)
			{
				const RenderGraphRenderPass &pass = *passes[passStack[p]];

				for (size_t i = 0; i < pass.getInputAttachmentInputs().size(); i++)
				{
					const VulkanRenderGraphTextureView &graphTextureView = graphTextureViews[pass.getInputAttachmentInputs()[i]];
					const std::string &inputAttachmentName = pass.getInputAttachmentInputs()[i];

					auto attachmentIt = imageViewPassAttachmentsIndex.find(inputAttachmentName);
				
					/*
					If there are no existing attachment descriptions for this, it means we need to load it from an external render pass,
					meaning the image isn't being rendered to in THIS render pass, but instead in a previous render pass.
					*/
					if (attachmentIt == imageViewPassAttachmentsIndex.end())
					{
						VkAttachmentDescription attachmentDesc = {};
						attachmentDesc.flags = 0;
						attachmentDesc.format = ResourceFormatToVkFormat(graphTextureView.attachment.format);
						attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
						attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
						attachmentDesc.storeOp = endPassStackIndex >= attachmentUsageLifetimes[inputAttachmentName].y ? VK_ATTACHMENT_STORE_OP_DONT_CARE : VK_ATTACHMENT_STORE_OP_STORE;
						attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
						attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
						attachmentDesc.initialLayout = textureStates[inputAttachmentName].currentLayout;
						attachmentDesc.finalLayout = isDepthFormat(graphTextureView.attachment.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

						passAttachments.push_back(attachmentDesc);
						passAttachmentImageViews.push_back(inputAttachmentName);
						passData.clearValues.push_back({0.0f, 0.0f, 0.0f, 0.0f});
						imageViewPassAttachmentsIndex[inputAttachmentName] = passAttachments.size() - 1;
						textureStates[inputAttachmentName].currentLayout = attachmentDesc.finalLayout;
					}
				}
			}

			// TODO Search for next resource layout and retroactively change finalLayouts of attachment descriptions

			// Build the subpass descriptions and attachment references
			for (size_t p = basePassStackIndex; p <= passStackIndex; p++)
			{
				const RenderGraphRenderPass &pass = *passes[passStack[p]];
				std::vector<VkAttachmentReference> &attachmentRefs = subpassAttachmentRefs[p - basePassStackIndex];
				std::vector<uint32_t> &preserveAttachments = subpassPreserveAttachments[p - basePassStackIndex];

				size_t inputAttachmentVectorOffset = attachmentRefs.size();
				for (size_t i = 0; i < pass.getInputAttachmentInputs().size(); i++)
				{
					VkAttachmentReference attachmentRef = {};
					attachmentRef.attachment = imageViewPassAttachmentsIndex[pass.getInputAttachmentInputs()[i]];
					attachmentRef.layout = isDepthFormat(graphTextureViews[pass.getInputAttachmentInputs()[i]].attachment.format) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

					attachmentRefs.push_back(attachmentRef);
				}

				size_t colorAttachmentVectorOffset = attachmentRefs.size();
				for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
				{
					VkAttachmentReference attachmentRef = {};
					attachmentRef.attachment = imageViewPassAttachmentsIndex[pass.getColorAttachmentOutputs()[o].textureName];
					attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

					attachmentRefs.push_back(attachmentRef);
				}

				size_t depthStencilAttachmentVectorOffset = attachmentRefs.size();
				if (pass.hasDepthStencilOutput())
				{
					VkAttachmentReference attachmentRef = {};
					attachmentRef.attachment = imageViewPassAttachmentsIndex[pass.getDepthStencilAttachmentOutput().textureName];
					attachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

					attachmentRefs.push_back(attachmentRef);
				}

				/*
				I'm doing preserve attachments by going through the list of all attachments (all VkAttachmentDescriptions) and
				checking if a) this pass stack index is within it's lifetime and b) it's not mentioned by any other attachment refs. 
				If a and b are true then we need to add this to the list of preserve attachments
				*/
				size_t preserveAttachmentCount = 0;
				for (auto attachmentIt = imageViewPassAttachmentsIndex.begin(); attachmentIt != imageViewPassAttachmentsIndex.end(); attachmentIt++)
				{
					if (p < attachmentUsageLifetimes[attachmentIt->first].x || p > attachmentUsageLifetimes[attachmentIt->first].y)
						continue;

					if (std::find(pass.getInputAttachmentInputs().begin(), pass.getInputAttachmentInputs().end(), attachmentIt->first) != pass.getInputAttachmentInputs().end())
						continue;

					bool foundColorAttachmentMatch = false;
					for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
					{
						if (attachmentIt->first == pass.getColorAttachmentOutputs()[o].textureName)
						{
							foundColorAttachmentMatch = true;
							break;
						}
					}

					if (foundColorAttachmentMatch)
						continue;

					if (pass.hasDepthStencilOutput() && attachmentIt->first == pass.getDepthStencilAttachmentOutput().textureName)
						continue;

					preserveAttachments.push_back(attachmentIt->second);
					preserveAttachmentCount++;
				}

				VkSubpassDescription subpassDesc = {};
				subpassDesc.flags = 0;
				subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpassDesc.inputAttachmentCount = (uint32_t) pass.getInputAttachmentInputs().size();
				subpassDesc.pInputAttachments = subpassDesc.inputAttachmentCount > 0 ? &attachmentRefs[inputAttachmentVectorOffset] : nullptr;
				subpassDesc.colorAttachmentCount = (uint32_t) pass.getColorAttachmentOutputs().size();
				subpassDesc.pColorAttachments = subpassDesc.colorAttachmentCount > 0 ? &attachmentRefs[colorAttachmentVectorOffset] : nullptr;
				subpassDesc.pResolveAttachments = nullptr;
				subpassDesc.pDepthStencilAttachment = pass.hasDepthStencilOutput() ? &attachmentRefs[depthStencilAttachmentVectorOffset] : nullptr;
				subpassDesc.preserveAttachmentCount = (uint32_t) preserveAttachmentCount;
				subpassDesc.pPreserveAttachments = subpassDesc.preserveAttachmentCount > 0 ? &preserveAttachments[0] : nullptr;

				passSubpasses.push_back(subpassDesc);
			}

			// Define the subpass dependencies
			for (int64_t p = int64_t(passStackIndex); p >= int64_t(basePassStackIndex); p--)
			{
				const RenderGraphRenderPass &pass = *passes[passStack[p]];

				for (size_t i = 0; i < pass.getInputAttachmentInputs().size(); i++)
				{
					const std::string &inputAttachment = pass.getInputAttachmentInputs()[i];

					for (size_t p1 = basePassStackIndex; p1 <= passStackIndex; p1++)
					{
						if (p1 >= p)
							continue;

						bool foundColorSubpassWrite = false;
						bool foundDepthSubpassWrite = false;

						for (size_t o = 0; o < pass.getColorAttachmentOutputs().size() && !foundColorSubpassWrite; o++)
							if (inputAttachment == pass.getColorAttachmentOutputs()[o].textureName)
								foundColorSubpassWrite = true;

						if (pass.hasDepthStencilOutput() && inputAttachment == pass.getDepthStencilAttachmentOutput().textureName)
							foundDepthSubpassWrite = true;

						if (foundColorSubpassWrite || foundDepthSubpassWrite)
						{
							VkSubpassDependency dependencyDesc = {};
							dependencyDesc.srcSubpass = (uint32_t) (p1 - basePassStackIndex);
							dependencyDesc.dstSubpass = (uint32_t) (p - basePassStackIndex);
							dependencyDesc.srcStageMask = (foundColorSubpassWrite ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : 0) | (foundDepthSubpassWrite ? VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT : 0);
							dependencyDesc.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
							dependencyDesc.srcAccessMask = (foundColorSubpassWrite ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0) | (foundDepthSubpassWrite ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0);
							dependencyDesc.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

							passDependencies.push_back(dependencyDesc);
						}
					}
				}
			}

			VkRenderPassCreateInfo renderPassCreateInfo = {};
			renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassCreateInfo.pNext = nullptr;
			renderPassCreateInfo.flags = 0;
			renderPassCreateInfo.attachmentCount = (uint32_t) passAttachments.size();
			renderPassCreateInfo.pAttachments = passAttachments.data();
			renderPassCreateInfo.subpassCount = (uint32_t) passSubpasses.size();
			renderPassCreateInfo.pSubpasses = passSubpasses.data();
			renderPassCreateInfo.dependencyCount = (uint32_t) passDependencies.size();
			renderPassCreateInfo.pDependencies = passDependencies.data();

			VK_CHECK_RESULT(vkCreateRenderPass(renderer->device, &renderPassCreateInfo, nullptr, &passData.renderPassHandle));

			std::vector<VkImageView> framebufferVkImageViews;

			for (const std::string &textureViewName : passAttachmentImageViews)
				framebufferVkImageViews.push_back(static_cast<VulkanTextureView *>(graphTextureViews[textureViewName].textureView)->imageView);
			
			passData.framebufferTextureViews = passAttachmentImageViews;

			VkFramebufferCreateInfo framebufferCreateInfo = {};
			framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferCreateInfo.pNext = nullptr;
			framebufferCreateInfo.flags = 0;
			framebufferCreateInfo.renderPass = passData.renderPassHandle;
			framebufferCreateInfo.attachmentCount = (uint32_t)framebufferVkImageViews.size();
			framebufferCreateInfo.pAttachments = framebufferVkImageViews.data();
			framebufferCreateInfo.width = passData.renderArea.x;
			framebufferCreateInfo.height = passData.renderArea.y;
			framebufferCreateInfo.layers = passData.renderArea.z;

			VK_CHECK_RESULT(vkCreateFramebuffer(renderer->device, &framebufferCreateInfo, nullptr, &passData.framebufferHandle));

			VulkanRenderPass tempRenderPass = {};
			tempRenderPass.renderPassHandle = passData.renderPassHandle;

			for (size_t p = basePassStackIndex; p <= passStackIndex; p++)
			{
				RenderGraphInitFunctionData initData = {};
				initData.renderPassHandle = &tempRenderPass;
				initData.baseSubpass = 0;

				if (passes[passStack[p]]->hasInitFunction())
					passes[passStack[p]]->getInitFunction()(initData);
			}
		}
		else if (passData.pipelineType == RENDER_GRAPH_PIPELINE_TYPE_COMPUTE || passData.pipelineType == RENDER_GRAPH_PIPELINE_TYPE_GENERAL)
		{
			const RenderGraphRenderPass &pass = *passes[passStack[passStackIndex]];

			for (size_t i = 0; i < pass.getSampledTextureInputs().size(); i++)
			{
				const std::string &textureName = pass.getSampledTextureInputs()[i];
				bool textureIsInDepthFormat = isDepthFormat(graphTextureViews[textureName].textureView->viewFormat);
				VkImageLayout currentLayout = textureStates[textureName].currentLayout;
				VkImageLayout requiredLayout = textureIsInDepthFormat ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

				if (currentLayout != requiredLayout)
				{
					VkImageMemoryBarrier barrier = {};
					barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					barrier.pNext = nullptr;
					barrier.srcAccessMask = getAccessFlagsForImageLayoutTransition(currentLayout);
					barrier.dstAccessMask = getAccessFlagsForImageLayoutTransition(requiredLayout);
					barrier.oldLayout = currentLayout;
					barrier.newLayout = requiredLayout;
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.image = static_cast<VulkanTexture*>(graphTextureViews[textureName].textureView->parentTexture)->imageHandle;
					barrier.subresourceRange.aspectMask = textureIsInDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
					barrier.subresourceRange.baseMipLevel = 0;
					barrier.subresourceRange.levelCount = graphTextureViews[textureName].attachment.mipLevels;
					barrier.subresourceRange.baseArrayLayer = 0;
					barrier.subresourceRange.layerCount = graphTextureViews[textureName].attachment.arrayLayers;

					passData.beforeRenderImageBarriers.push_back(barrier);
				}

				textureStates[textureName].currentLayout = requiredLayout;
			}

			for (size_t st = 0; st < pass.getStorageTextures().size(); st++)
			{
				const RenderPassStorageTexture &storageTexture = pass.getStorageTextures()[st];
				bool textureIsInDepthFormat = isDepthFormat(graphTextureViews[storageTexture.textureName].textureView->viewFormat);
				VkImageLayout currentLayout = textureStates[storageTexture.textureName].currentLayout;
				VkImageLayout requiredLayout = toVkImageLayout(storageTexture.passBeginLayout);

				if (currentLayout != requiredLayout)
				{
					VkImageMemoryBarrier barrier = {};
					barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					barrier.pNext = nullptr;
					barrier.srcAccessMask = getAccessFlagsForImageLayoutTransition(currentLayout);
					barrier.dstAccessMask = getAccessFlagsForImageLayoutTransition(requiredLayout);
					barrier.oldLayout = currentLayout;
					barrier.newLayout = requiredLayout;
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.image = static_cast<VulkanTexture*>(graphTextureViews[storageTexture.textureName].textureView->parentTexture)->imageHandle;
					barrier.subresourceRange.aspectMask = textureIsInDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
					barrier.subresourceRange.baseMipLevel = 0;
					barrier.subresourceRange.levelCount = graphTextureViews[storageTexture.textureName].attachment.mipLevels;
					barrier.subresourceRange.baseArrayLayer = 0;
					barrier.subresourceRange.layerCount = graphTextureViews[storageTexture.textureName].attachment.arrayLayers;

					passData.beforeRenderImageBarriers.push_back(barrier);
				}

				textureStates[storageTexture.textureName].currentLayout = toVkImageLayout(storageTexture.passEndLayout);

				if (storageTexture.textureName == outputAttachmentName)
				{
					VkImageMemoryBarrier barrier = {};
					barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
					barrier.pNext = nullptr;
					barrier.srcAccessMask = getAccessFlagsForImageLayoutTransition(requiredLayout);
					barrier.dstAccessMask = getAccessFlagsForImageLayoutTransition(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
					barrier.oldLayout = requiredLayout;
					barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.image = static_cast<VulkanTexture*>(graphTextureViews[storageTexture.textureName].textureView->parentTexture)->imageHandle;
					barrier.subresourceRange.aspectMask = textureIsInDepthFormat ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
					barrier.subresourceRange.baseMipLevel = 0;
					barrier.subresourceRange.levelCount = graphTextureViews[storageTexture.textureName].attachment.mipLevels;
					barrier.subresourceRange.baseArrayLayer = 0;
					barrier.subresourceRange.layerCount = graphTextureViews[storageTexture.textureName].attachment.arrayLayers;

					passData.afterRenderImageBarriers.push_back(barrier);

					textureStates[storageTexture.textureName].currentLayout = barrier.newLayout;
				}
			}

			for (size_t sb = 0; sb < pass.getStorageBuffers().size(); sb++)
			{
				const RenderPassStorageBuffer &storageBuffer = pass.getStorageBuffers()[sb];
				BufferLayout currentLayout = bufferStates[storageBuffer.bufferName].currentLayout;
				BufferLayout requiredLayout = storageBuffer.passBeginLayout;

				if (currentLayout != requiredLayout && currentLayout != BUFFER_LAYOUT_MAX_ENUM)
				{
					VkBufferMemoryBarrier barrier = {};
					barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					barrier.pNext = nullptr;
					barrier.srcAccessMask = getAccessFlagsForBufferLayoutTransition(currentLayout);
					barrier.dstAccessMask = getAccessFlagsForBufferLayoutTransition(requiredLayout);
					barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					barrier.buffer = graphBuffers[storageBuffer.bufferName]->bufferHandle;
					barrier.offset = 0;
					barrier.size = storageBuffer.size;

					passData.beforeRenderBufferBarriers.push_back(barrier);

					bufferStates[storageBuffer.bufferName].currentLayout = storageBuffer.passEndLayout;
				}
			}

			passData.passIndices.push_back(passStack[passStackIndex]);

			RenderGraphInitFunctionData initData = {};
			initData.renderPassHandle = nullptr;
			initData.baseSubpass = 0;

			if (passes[passStack[passStackIndex]]->hasInitFunction())
				passes[passStack[passStackIndex]]->getInitFunction()(initData);
		}

		finalRenderPasses.push_back(passData);
	}

	RenderGraphDescriptorUpdateFunctionData descUpdateData = {};
	
	for (auto textureViewIt = graphTextureViews.begin(); textureViewIt != graphTextureViews.end(); textureViewIt++)
		descUpdateData.graphTextureViews[textureViewIt->first] = textureViewIt->second.textureView;

	for (auto bufferIt = graphBuffers.begin(); bufferIt != graphBuffers.end(); bufferIt++)
		descUpdateData.graphBuffers[bufferIt->first] = bufferIt->second;

	for (size_t i = 0; i < finalRenderPasses.size(); i++)
		for (size_t p = 0; p < finalRenderPasses[i].passIndices.size(); p++)
			if (passes[finalRenderPasses[i].passIndices[p]]->hasDescriptorUpdateFunction())
				passes[finalRenderPasses[i].passIndices[p]]->getDescriptorUpdateFunction()(descUpdateData);
}

TextureView VulkanRenderGraph::getRenderGraphOutputTextureView()
{
	return graphTextureViews[outputAttachmentName].textureView;
}

#endif