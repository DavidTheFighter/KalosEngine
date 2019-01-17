#include "RendererCore/Vulkan/VulkanCommandBuffer.h"

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

void VulkanCommandBuffer::pushConstants (ShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data)
{
	vkCmdPushConstants(bufferHandle, static_cast<VulkanPipeline*>(context_currentBoundPipeline)->pipelineLayoutHandle, toVkShaderStageFlags(stages), offset, size, data);
}

void VulkanCommandBuffer::bindDescriptorSets (PipelineBindPoint point, uint32_t firstSet, std::vector<DescriptorSet> sets)
{
	std::vector<VkDescriptorSet> vulkanSets;

	for (size_t i = 0; i < sets.size(); i ++)
	{
		vulkanSets.push_back(static_cast<VulkanDescriptorSet*>(sets[i])->setHandle);
	}

	vkCmdBindDescriptorSets(bufferHandle, toVkPipelineBindPoint(point), static_cast<VulkanPipeline*>(context_currentBoundPipeline)->pipelineLayoutHandle, firstSet, static_cast<uint32_t>(sets.size()), vulkanSets.data(), 0, nullptr);
}

void VulkanCommandBuffer::transitionTextureLayout (Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = toVkImageLayout(oldLayout);
	barrier.newLayout = toVkImageLayout(newLayout);
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = static_cast<VulkanTexture*>(texture)->imageHandle;
	barrier.subresourceRange =
	{ VkAccessFlags(isDepthFormat(texture->textureFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), subresource.baseMipLevel, subresource.levelCount, subresource.baseArrayLayer, subresource.layerCount};

	VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

	switch (oldLayout)
	{
		case TEXTURE_LAYOUT_UNDEFINED:
		{
			barrier.srcAccessMask = 0;
			srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

			break;
		}
		case TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL:
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			break;
		}
		case TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			break;
		}
		default:
			break;
	}

	switch (newLayout)
	{
		case TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL:
		{
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

			break;
		}
		case TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
		{
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

			break;
		}
		case TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL:
		{
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			dstStage = VK_PIPELINE_STAGE_HOST_BIT;

			break;
		}
		default:
			break;
	}

	vkCmdPipelineBarrier(bufferHandle, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanCommandBuffer::setTextureLayout (Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource, PipelineStageFlags srcStage, PipelineStageFlags dstStage)
{
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.oldLayout = toVkImageLayout(oldLayout);
	imageMemoryBarrier.newLayout = toVkImageLayout(newLayout);
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.image = static_cast<VulkanTexture*>(texture)->imageHandle;
	imageMemoryBarrier.subresourceRange =
	{ VkImageAspectFlags(isDepthFormat(texture->textureFormat) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT), subresource.baseMipLevel, subresource.levelCount, subresource.baseArrayLayer, subresource.layerCount};

	// Source layouts (old)
	// Source access mask controls actions that have to be finished on the old layout
	// before it will be transitioned to the new layout
	switch (oldLayout)
	{
		case TEXTURE_LAYOUT_UNDEFINED:
			// Image layout is undefined (or does not matter)
			// Only valid as initial layout
			// No flags required, listed only for completeness
			imageMemoryBarrier.srcAccessMask = 0;
			break;

		case TEXTURE_LAYOUT_PREINITIALIZED:
			// Image is preinitialized
			// Only valid as initial layout for linear images, preserves memory contents
			// Make sure host writes have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
			break;

		case TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			// Image is a color attachment
			// Make sure any writes to the color buffer have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			// Image is a depth/stencil attachment
			// Make sure any writes to the depth/stencil buffer have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			// Image is a transfer source
			// Make sure any reads from the image have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;

		case TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL:
			// Image is a transfer destination
			// Make sure any writes to the image have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

		case TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			// Image is read by a shader
			// Make sure any shader reads from the image have been finished
			imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			// Other source layouts aren't handled (yet)
			break;
	}

	// Target layouts (new)
	// Destination access mask controls the dependency for the new image layout
	switch (newLayout)
	{
		case TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL:
			// Image will be used as a transfer destination
			// Make sure any writes to the image have been finished
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			break;

		case TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			// Image will be used as a transfer source
			// Make sure any reads from the image have been finished
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			break;

		case TEXTURE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			// Image will be used as a color attachment
			// Make sure any writes to the color buffer have been finished
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			break;

		case TEXTURE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			// Image layout will be used as a depth/stencil attachment
			// Make sure any writes to depth/stencil buffer have been finished
			imageMemoryBarrier.dstAccessMask = imageMemoryBarrier.dstAccessMask | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			break;

		case TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			// Image will be read in a shader (sampler, input attachment)
			// Make sure any writes to the image have been finished
			if (imageMemoryBarrier.srcAccessMask == 0)
			{
				imageMemoryBarrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
			}
			imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			break;
		default:
			// Other source layouts aren't handled (yet)
			break;
	}

	vkCmdPipelineBarrier(bufferHandle, toVkPipelineStageFlags(srcStage), toVkPipelineStageFlags(dstStage), 0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
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
	imgCopyRegion.imageOffset =
	{	offset.x, offset.y, offset.z};
	imgCopyRegion.imageExtent =
	{	extent.x == std::numeric_limits<uint32_t>::max() ? dstTexture->width : extent.x,
		extent.y == std::numeric_limits<uint32_t>::max() ? dstTexture->height : extent.y,
		extent.z == std::numeric_limits<uint32_t>::max() ? dstTexture->depth : extent.z};

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
