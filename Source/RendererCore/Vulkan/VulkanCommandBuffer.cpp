#include "RendererCore/Vulkan/VulkanCommandBuffer.h"
#if BUILD_VULKAN_BACKEND

#include <RendererCore/Vulkan/VulkanEnums.h>
#include <RendererCore/Vulkan/VulkanObjects.h>

VulkanCommandBuffer::VulkanCommandBuffer()
{
	bufferHandle = nullptr;
	context_currentBoundPipeline = nullptr;

	startedRecording = false;
}

VulkanCommandBuffer::~VulkanCommandBuffer ()
{

}

void VulkanCommandBuffer::beginCommands (CommandBufferUsageFlags flags)
{
	if (startedRecording)
	{
		Log::get()->error("VulkanCommandBuffer: Cannot record to a command buffer that has already been recorded too. Reset it first or allocate a new one");

		return;
	}

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = toVkCommandBufferUsageFlags(flags);

	VK_CHECK_RESULT(vkBeginCommandBuffer(bufferHandle, &beginInfo));

	startedRecording = true;
}

void VulkanCommandBuffer::endCommands ()
{
	VK_CHECK_RESULT(vkEndCommandBuffer(bufferHandle));
}

void VulkanCommandBuffer::vulkan_resetCommands ()
{
	VK_CHECK_RESULT(vkResetCommandBuffer(bufferHandle, 0));
}

void VulkanCommandBuffer::bindPipeline (PipelineBindPoint point, Pipeline pipeline)
{
	vkCmdBindPipeline(bufferHandle, toVkPipelineBindPoint(point), static_cast<VulkanPipeline*>(pipeline)->pipelineHandle);

	context_currentBoundPipeline = pipeline;
}

void VulkanCommandBuffer::bindIndexBuffer (Buffer buffer, size_t offset, bool uses32BitIndices)
{
	vkCmdBindIndexBuffer(bufferHandle, static_cast<VulkanBuffer*>(buffer)->bufferHandle, static_cast<VkDeviceSize>(offset), uses32BitIndices ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
}

void VulkanCommandBuffer::bindVertexBuffers (uint32_t firstBinding, const std::vector<Buffer> &buffers, const std::vector<size_t> &offsets)
{
	DEBUG_ASSERT(buffers.size() == offsets.size());

	std::vector<VkBuffer> vulkanBuffers;
	std::vector<VkDeviceSize> vulkanOffsets;

	for (size_t i = 0; i < buffers.size(); i ++)
	{
		vulkanBuffers.push_back(static_cast<VulkanBuffer*>(buffers[i])->bufferHandle);
		vulkanOffsets.push_back(static_cast<VkDeviceSize>(offsets[i]));
	}

	vkCmdBindVertexBuffers(bufferHandle, firstBinding, static_cast<uint32_t>(buffers.size()), vulkanBuffers.data(), vulkanOffsets.data());
}

void VulkanCommandBuffer::draw (uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
	vkCmdDraw(bufferHandle, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandBuffer::drawIndexed (uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t firstVertex, uint32_t firstInstance)
{
	vkCmdDrawIndexed(bufferHandle, indexCount, instanceCount, firstIndex, firstVertex, firstInstance);
}

void VulkanCommandBuffer::dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ)
{
	vkCmdDispatch(bufferHandle, groupCountX, groupCountY, groupCountZ);
}

void VulkanCommandBuffer::resolveTexture(Texture srcTexture, Texture dstTexture, TextureSubresourceRange subresources)
{
	VkImageAspectFlags imageAspect = isDepthFormat(srcTexture->textureFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

	std::vector<VkImageResolve> regions(subresources.levelCount);

	for (uint32_t level = subresources.baseMipLevel; level < subresources.baseMipLevel + subresources.levelCount; level++)
	{
		VkImageResolve region = {};
		region.srcSubresource = {imageAspect, level, subresources.baseArrayLayer, subresources.layerCount};
		region.srcOffset = {0, 0, 0};
		region.dstSubresource = {imageAspect, level, subresources.baseArrayLayer, subresources.layerCount};
		region.dstOffset = {0, 0, 0};
		region.extent = {srcTexture->width, srcTexture->height, srcTexture->depth};

		regions[level - subresources.baseMipLevel] = region;
	}

	vkCmdResolveImage(bufferHandle, static_cast<VulkanTexture*>(srcTexture)->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, static_cast<VulkanTexture*>(dstTexture)->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t) regions.size(), regions.data());
}

void VulkanCommandBuffer::pushConstants (uint32_t offset, uint32_t size, const void *data)
{
	vkCmdPushConstants(bufferHandle, static_cast<VulkanPipeline*>(context_currentBoundPipeline)->pipelineLayoutHandle, toVkShaderStageFlags(context_currentBoundPipeline->gfxPipelineInfo.inputPushConstants.stageAccessFlags | context_currentBoundPipeline->computePipelineInfo.inputPushConstants.stageAccessFlags), offset, size, data);
}

void VulkanCommandBuffer::bindDescriptorSets (PipelineBindPoint point, uint32_t firstSet, const std::vector<DescriptorSet> &sets)
{
	std::vector<VkDescriptorSet> vulkanSets;

	for (size_t i = 0; i < sets.size(); i ++)
	{
		vulkanSets.push_back(static_cast<VulkanDescriptorSet*>(sets[i])->setHandle);
	}

	vkCmdBindDescriptorSets(bufferHandle, toVkPipelineBindPoint(point), static_cast<VulkanPipeline*>(context_currentBoundPipeline)->pipelineLayoutHandle, firstSet, static_cast<uint32_t>(sets.size()), vulkanSets.data(), 0, nullptr);
}

void VulkanCommandBuffer::resourceBarriers(const std::vector<ResourceBarrier> &barriers)
{
	std::vector<VkImageMemoryBarrier> imageBarriers;
	std::vector<VkBufferMemoryBarrier> bufferBarriers;
	std::vector<VkMemoryBarrier> memoryBarriers;

	VkPipelineStageFlags srcStageMask = 0;
	VkPipelineStageFlags dstStageMask = 0;

	for (size_t i = 0; i < barriers.size(); i++)
	{
		const ResourceBarrier &barrierInfo = barriers[i];

		switch (barrierInfo.barrierType)
		{
			case RESOURCE_BARRIER_TYPE_TEXTURE_TRANSITION:
			{
				const TextureTransitionBarrier &textureBarrierInfo = barrierInfo.textureTransition;

				// Perform layout validation for the texture's usage flags

				VkImageMemoryBarrier imageBarrier = {};
				imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
				imageBarrier.pNext = nullptr;
				imageBarrier.srcAccessMask = getAccessFlagsForImageLayoutTransition(toVkImageLayout(textureBarrierInfo.oldLayout));
				imageBarrier.dstAccessMask = getAccessFlagsForImageLayoutTransition(toVkImageLayout(textureBarrierInfo.newLayout));
				imageBarrier.oldLayout = toVkImageLayout(textureBarrierInfo.oldLayout);
				imageBarrier.newLayout = toVkImageLayout(textureBarrierInfo.newLayout);
				imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				imageBarrier.image = static_cast<VulkanTexture*>(textureBarrierInfo.texture)->imageHandle;
				imageBarrier.subresourceRange.aspectMask = isDepthFormat(textureBarrierInfo.texture->textureFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
				imageBarrier.subresourceRange.baseMipLevel = textureBarrierInfo.subresourceRange.baseMipLevel;
				imageBarrier.subresourceRange.levelCount = textureBarrierInfo.subresourceRange.levelCount;
				imageBarrier.subresourceRange.baseArrayLayer = textureBarrierInfo.subresourceRange.baseArrayLayer;
				imageBarrier.subresourceRange.layerCount = textureBarrierInfo.subresourceRange.layerCount;

				srcStageMask |= getPipelineStagesForImageLayout(toVkImageLayout(textureBarrierInfo.oldLayout));
				dstStageMask |= getPipelineStagesForImageLayout(toVkImageLayout(textureBarrierInfo.newLayout));

				imageBarriers.push_back(imageBarrier);
				break;
			}
			case RESOURCE_BARRIER_TYPE_BUFFER_TRANSITION:
			{
				const BufferTransitionBarrier &bufferBarrierInfo = barrierInfo.bufferTransition;

				// TODO Perform layout validation for the buffer's usage flags

				VkBufferMemoryBarrier bufferBarrier = {};
				bufferBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
				bufferBarrier.pNext = nullptr;
				bufferBarrier.srcAccessMask = getAccessFlagsForBufferLayoutTransition(bufferBarrierInfo.oldLayout);
				bufferBarrier.dstAccessMask = getAccessFlagsForBufferLayoutTransition(bufferBarrierInfo.newLayout);
				bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
				bufferBarrier.buffer = static_cast<VulkanBuffer*>(bufferBarrierInfo.buffer)->bufferHandle;
				bufferBarrier.offset = 0;
				bufferBarrier.size = bufferBarrierInfo.buffer->bufferSize;

				srcStageMask |= getPipelineStagesForBufferLayout(bufferBarrierInfo.oldLayout);
				dstStageMask |= getPipelineStagesForBufferLayout(bufferBarrierInfo.newLayout);

				bufferBarriers.push_back(bufferBarrier);
				break;
			}
		}
	}

	vkCmdPipelineBarrier(bufferHandle, srcStageMask, dstStageMask, 0, (uint32_t) memoryBarriers.size(), memoryBarriers.data(), (uint32_t) bufferBarriers.size(), bufferBarriers.data(), (uint32_t) imageBarriers.size(), imageBarriers.data());
}

void VulkanCommandBuffer::stageBuffer (StagingBuffer stagingBuffer, Texture dstTexture, TextureSubresourceLayers subresource, sivec3 offset, suvec3 extent)
{
	VkBufferImageCopy imgCopyRegion = {};
	imgCopyRegion.bufferOffset = 0;
	imgCopyRegion.bufferRowLength = 0;
	imgCopyRegion.bufferImageHeight = 0;
	imgCopyRegion.imageSubresource.aspectMask = isDepthFormat(dstTexture->textureFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imgCopyRegion.imageSubresource.mipLevel = subresource.mipLevel;
	imgCopyRegion.imageSubresource.baseArrayLayer = subresource.baseArrayLayer;
	imgCopyRegion.imageSubresource.layerCount = subresource.layerCount;
	imgCopyRegion.imageOffset = {offset.x, offset.y, offset.z};
	imgCopyRegion.imageExtent = {
		extent.x == std::numeric_limits<uint32_t>::max() ? dstTexture->width : extent.x,
		extent.y == std::numeric_limits<uint32_t>::max() ? dstTexture->height : extent.y,
		extent.z == std::numeric_limits<uint32_t>::max() ? dstTexture->depth : extent.z
	};

	vkCmdCopyBufferToImage(bufferHandle, static_cast<VulkanStagingBuffer*>(stagingBuffer)->bufferHandle, static_cast<VulkanTexture*>(dstTexture)->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imgCopyRegion);
}

void VulkanCommandBuffer::stageBuffer (StagingBuffer stagingBuffer, Buffer dstBuffer)
{
	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.size = static_cast<VulkanStagingBuffer*>(stagingBuffer)->bufferSize;

	vkCmdCopyBuffer(bufferHandle, static_cast<VulkanStagingBuffer*>(stagingBuffer)->bufferHandle, static_cast<VulkanBuffer*>(dstBuffer)->bufferHandle, 1, &bufferCopyRegion);
}

void VulkanCommandBuffer::stageTextureSubresources(StagingTexture stagingTexture, Texture dstTexture, TextureSubresourceRange subresources)
{
	VulkanStagingTexture *vkStagingTexture = static_cast<VulkanStagingTexture*>(stagingTexture);
	VulkanTexture *vkDstTexture = static_cast<VulkanTexture*>(dstTexture);

	std::vector<VkBufferImageCopy> copyRegions;

	for (uint32_t layer = subresources.baseArrayLayer; layer < subresources.baseArrayLayer + subresources.layerCount; layer++)
	{
		for (uint32_t level = subresources.baseMipLevel; level < subresources.baseMipLevel + subresources.levelCount; level++)
		{
			uint32_t subresourceIndex = layer * vkStagingTexture->mipLevels + level;

			VkBufferImageCopy copyInfo = {};
			copyInfo.bufferOffset = vkStagingTexture->subresourceOffets[subresourceIndex];
			copyInfo.bufferRowLength = std::max<uint32_t>(vkStagingTexture->width >> level, 1);// vkStagingTexture->subresourceRowPitches[subresourceIndex];
			copyInfo.bufferImageHeight = std::max<uint32_t>(vkStagingTexture->height >> level, 1);
			copyInfo.imageSubresource = {(VkImageAspectFlags) (isDepthFormat(vkDstTexture->textureFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), level, layer, 1};
			copyInfo.imageOffset = {0, 0, 0};
			copyInfo.imageExtent = {std::max<uint32_t>(vkStagingTexture->width >> level, 1), std::max<uint32_t>(vkStagingTexture->height >> level, 1), std::max<uint32_t>(vkStagingTexture->depth >> level, 1)};

			copyRegions.push_back(copyInfo);
		}
	}

	vkCmdCopyBufferToImage(bufferHandle, vkStagingTexture->bufferHandle, vkDstTexture->imageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, (uint32_t) copyRegions.size(), copyRegions.data());
}

void VulkanCommandBuffer::setViewports (uint32_t firstViewport, const std::vector<Viewport> &viewports)
{
	std::vector<VkViewport> vulkanViewports;

	for (const Viewport &viewport : viewports)
	{
		VkViewport vulkanViewPort = {};
		vulkanViewPort.x = viewport.x;
		vulkanViewPort.width = viewport.width;
		vulkanViewPort.minDepth = viewport.minDepth;
		vulkanViewPort.maxDepth = viewport.maxDepth;

		vulkanViewPort.y = viewport.height;
		vulkanViewPort.height = -viewport.height;

		vulkanViewports.push_back(vulkanViewPort);
	}

	vkCmdSetViewport(bufferHandle, firstViewport, static_cast<uint32_t>(vulkanViewports.size()), vulkanViewports.data());
}

void VulkanCommandBuffer::setScissors (uint32_t firstScissor, const std::vector<Scissor> &scissors)
{
	// Generic scissors are laid out in the same way as VkRect2D's, so they should be fine to cast directly
	vkCmdSetScissor(bufferHandle, firstScissor, static_cast<uint32_t>(scissors.size()), reinterpret_cast<const VkRect2D*>(scissors.data()));
}

void VulkanCommandBuffer::blitTexture (Texture src, TextureLayout srcLayout, Texture dst, TextureLayout dstLayout, std::vector<TextureBlitInfo> blitRegions, SamplerFilter filter)
{
	std::vector<VkImageBlit> imageBlits;

	for (size_t i = 0; i < blitRegions.size(); i ++)
	{
		const TextureBlitInfo &blitRegion = blitRegions[i];
		VkImageBlit blit = {};

		blit.srcSubresource.aspectMask = isDepthFormat(src->textureFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.aspectMask = isDepthFormat(dst->textureFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		memcpy(reinterpret_cast<uint8_t*>(&blit.srcSubresource) + offsetof(VkImageSubresourceLayers, mipLevel), &blitRegion.srcSubresource, sizeof(blitRegion.srcSubresource));
		memcpy(reinterpret_cast<uint8_t*>(&blit.dstSubresource) + offsetof(VkImageSubresourceLayers, mipLevel), &blitRegion.dstSubresource, sizeof(blitRegion.dstSubresource));

		memcpy(blit.srcOffsets, blitRegion.srcOffsets, sizeof(blitRegion.srcOffsets));
		memcpy(blit.dstOffsets, blitRegion.dstOffsets, sizeof(blitRegion.dstOffsets));

		imageBlits.push_back(blit);
	}

	vkCmdBlitImage(bufferHandle, static_cast<VulkanTexture*>(src)->imageHandle, toVkImageLayout(srcLayout), static_cast<VulkanTexture*>(dst)->imageHandle, toVkImageLayout(dstLayout), static_cast<uint32_t>(imageBlits.size()), imageBlits.data(), toVkFilter(filter));
}

void VulkanCommandBuffer::beginDebugRegion (const std::string &regionName, glm::vec4 color)
{
#if RENDER_DEBUG_MARKERS

	if (VulkanExtensions::enabled_VK_EXT_debug_marker)
	{
		VkDebugMarkerMarkerInfoEXT info = {};
		info.pMarkerName = regionName.c_str();
		memcpy(info.color, &color.x, sizeof(color));

		vkCmdDebugMarkerBeginEXT(bufferHandle, &info);
	}

#endif
}

void VulkanCommandBuffer::endDebugRegion ()
{
#if RENDER_DEBUG_MARKERS

	if (VulkanExtensions::enabled_VK_EXT_debug_marker)
	{
		vkCmdDebugMarkerEndEXT(bufferHandle);
	}

#endif
}

void VulkanCommandBuffer::insertDebugMarker (const std::string &markerName, glm::vec4 color)
{
#if RENDER_DEBUG_MARKERS

	if (VulkanExtensions::enabled_VK_EXT_debug_marker)
	{
		VkDebugMarkerMarkerInfoEXT info = {};
		info.pMarkerName = markerName.c_str();
		memcpy(info.color, &color.x, sizeof(color));

		vkCmdDebugMarkerInsertEXT(bufferHandle, &info);
	}

#endif
}

void VulkanCommandBuffer::vulkan_beginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer, const VkRect2D &renderArea, const std::vector<VkClearValue> &clearValues, VkSubpassContents contents)
{
	VkRenderPassBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	beginInfo.renderPass = renderPass;
	beginInfo.framebuffer = framebuffer;
	beginInfo.renderArea = renderArea;
	beginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	beginInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(bufferHandle, &beginInfo, contents);
}

void VulkanCommandBuffer::vulkan_endRenderPass()
{
	vkCmdEndRenderPass(bufferHandle);
}

void VulkanCommandBuffer::vulkan_nextSubpass(VkSubpassContents contents)
{
	vkCmdNextSubpass(bufferHandle, contents);
}

#endif