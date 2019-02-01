#include "RendererCore/Vulkan/VulkanRenderGraph.h"
#if BUILD_VULKAN_BACKEND

#include <RendererCore/Vulkan/VulkanRenderer.h>
#include <RendererCore/Vulkan/VulkanEnums.h>

#include <RendererCore/RendererGraphHelper.h>

VulkanRenderGraph::VulkanRenderGraph(Renderer *rendererPtr)
{
	renderer = rendererPtr;
	execCounter = 0;

	enableSubpassMerging = true;

	for (size_t i = 0; i < 3; i++)
		gfxCommandPools.push_back(renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT));

	for (size_t i = 0; i < 3; i++)
		executionDoneSemaphores.push_back(renderer->createSemaphore());
}

VulkanRenderGraph::~VulkanRenderGraph()
{
	cleanupResources();
}

const std::string viewSelectMipPostfix = "_fg_miplvl";
const std::string viewSelectLayerPostfix = "_fg_layer";

TextureView VulkanRenderGraph::getRenderGraphOutputTextureView()
{
	return resourceImageViews[renderGraphOutputAttachment];
}

Semaphore VulkanRenderGraph::execute()
{
	VulkanCommandBuffer *cmdBuffer = static_cast<VulkanCommandBuffer*>(gfxCommandPools[execCounter % gfxCommandPools.size()]->allocateCommandBuffer());
	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	for (size_t i = 0; i < execCodes.size(); i++)
	{
		VulkanRenderGraphOpCode opCode = execCodes[i].first;
		void *opDataPtr = execCodes[i].second;

		switch (opCode)
		{
			case VKRG_OPCODE_BEGIN_RENDER_PASS:
			{
				const VulkanRenderGraphBeginRenderPassData &opData = *static_cast<VulkanRenderGraphBeginRenderPassData*>(opDataPtr);
				cmdBuffer->vulkan_beginRenderPass(opData.renderPass, opData.framebuffer, { 0, 0, opData.size.x, opData.size.y }, opData.clearValues, opData.subpassContents);
				cmdBuffer->setScissors(0, { {0, 0, opData.size.x, opData.size.y} });
				cmdBuffer->setViewports(0, { {0, 0, (float)opData.size.x, (float) opData.size.y, 0, 1} });

				break;
			}
			case VKRG_OPCODE_END_RENDER_PASS:
			{
				cmdBuffer->vulkan_endRenderPass();

				break;
			}
			case VKRG_OPCODE_NEXT_SUBPASS:
			{
				cmdBuffer->vulkan_nextSubpass(VK_SUBPASS_CONTENTS_INLINE);

				break;
			}
			case VKRG_OPCODE_POST_BLIT:
			{
				break;
				const VulkanRenderGraphPostBlitOpData &opData = *static_cast<VulkanRenderGraphPostBlitOpData*>(opDataPtr);
				VulkanRenderGraphImage graphTex = graphImages[opData.graphImageIndex];

				uint32_t sizeX = graphTex.attachment.namedRelativeSize == "" ? uint32_t(graphTex.attachment.sizeX) : uint32_t(namedSizes[graphTex.attachment.namedRelativeSize].x * graphTex.attachment.sizeX);
				uint32_t sizeY = graphTex.attachment.namedRelativeSize == "" ? uint32_t(graphTex.attachment.sizeY) : uint32_t(namedSizes[graphTex.attachment.namedRelativeSize].y * graphTex.attachment.sizeY);
				uint32_t sizeZ = (uint32_t) graphTex.attachment.sizeZ;

				/*
				cmdBuffer->setTextureLayout(graphTex.imageHandle, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL, { 0, 1, 0, graphTex.base.arrayLayers }, PIPELINE_STAGE_ALL_COMMANDS_BIT, PIPELINE_STAGE_TRANSFER_BIT);

				for (uint32_t m = 1; m < graphTex.base.mipLevels; m++)
				{
					TextureBlitInfo blitInfo = {};
					blitInfo.srcSubresource = { m - 1, 0, graphTex.base.arrayLayers };
					blitInfo.dstSubresource = { m, 0, graphTex.base.arrayLayers };
					blitInfo.srcOffsets[0] = { 0, 0, 0 };
					blitInfo.dstOffsets[0] = { 0, 0, 0 };
					blitInfo.srcOffsets[1] = { std::max(int32_t(sizeX >> (m - 1)), 1), std::max(int32_t(sizeY >> (m - 1)), 1), std::max(int32_t(sizeZ >> (m - 1)), 1) };
					blitInfo.dstOffsets[1] = { std::max(int32_t(sizeX >> m), 1), std::max(int32_t(sizeY >> m), 1), std::max(int32_t(sizeZ >> m), 1) };

					cmdBuffer->setTextureLayout(graphTex.attTex, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL, { m, 1, 0, graphTex.base.arrayLayers }, PIPELINE_STAGE_ALL_COMMANDS_BIT, PIPELINE_STAGE_TRANSFER_BIT);
					cmdBuffer->blitTexture(graphTex.attTex, TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL, graphTex.attTex, TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL, { blitInfo }, SAMPLER_FILTER_LINEAR);
					cmdBuffer->setTextureLayout(graphTex.attTex, TEXTURE_LAYOUT_TRANSFER_DST_OPTIMAL, TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL, { m, 1, 0, graphTex.base.arrayLayers }, PIPELINE_STAGE_TRANSFER_BIT, PIPELINE_STAGE_TRANSFER_BIT);
				}

				cmdBuffer->setTextureLayout(graphTex.attTex, TEXTURE_LAYOUT_TRANSFER_SRC_OPTIMAL, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, { 0, graphTex.base.mipLevels, 0, graphTex.base.arrayLayers }, PIPELINE_STAGE_TRANSFER_BIT, PIPELINE_STAGE_ALL_COMMANDS_BIT);
				*/
				break;
			}
			case VKRG_OPCODE_CALL_RENDER_FUNC:
			{
				const VulkanRenderGraphCallRenderFuncOpData &opData = *static_cast<VulkanRenderGraphCallRenderFuncOpData*>(opDataPtr);

				RenderGraphRenderFunctionData renderData = {};

				if (passes[opData.passIndex]->getPipelineType() == RG_PIPELINE_COMPUTE)
				{
					for (size_t o = 0; o < passes[opData.passIndex]->getColorOutputs().size(); o++)
					{
						//Texture output = resourceImageViews[passes[opData.passIndex]->getColorOutputs()[o].first]->parentTexture;
						
						//cmdBuffer->transitionTextureLayout(output, (TextureLayout) VK_IMAGE_LAYOUT_UNDEFINED, TEXTURE_LAYOUT_GENERAL, {0, output->mipCount, 0, output->layerCount});
					}
				}

				cmdBuffer->beginDebugRegion(passes[opData.passIndex]->getNodeName(), glm::vec4(0, 0.5f, 1, 1));
				passes[opData.passIndex]->getRenderFunction()(cmdBuffer, renderData);
				cmdBuffer->endDebugRegion();

				if (passes[opData.passIndex]->getPipelineType() == RG_PIPELINE_COMPUTE)
				{
					for (size_t o = 0; o < passes[opData.passIndex]->getColorOutputs().size(); o++)
					{
						//Texture output = resourceImageViews[passes[opData.passIndex]->getColorOutputs()[o].first]->parentTexture;

						//cmdBuffer->transitionTextureLayout(output, TEXTURE_LAYOUT_GENERAL, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, {0, output->mipCount, 0, output->layerCount});
					}
				}

				break;
			}
		}
	}

	cmdBuffer->endCommands();

	std::vector<PipelineStageFlags> waitSemStages = {};
	std::vector<Semaphore> waitSems = {};
	std::vector<Semaphore> signalSems = {executionDoneSemaphores[execCounter % executionDoneSemaphores.size()]};

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, waitSems, waitSemStages, {});
	renderer->waitForQueueIdle(QUEUE_TYPE_GRAPHICS);

	gfxCommandPools[execCounter % gfxCommandPools.size()]->resetCommandPoolAndFreeCommandBuffer(cmdBuffer);

	execCounter++;

	return signalSems[0];
}

void VulkanRenderGraph::resizeNamedSize(const std::string &sizeName, glm::uvec2 newSize)
{
	renderer->waitForDeviceIdle();

	cleanupResources();

	namedSizes[sizeName] = glm::uvec3(newSize, 1);

	execCounter = 0;

	for (size_t i = 0; i < 3; i++)
		gfxCommandPools.push_back(renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT));

	for (size_t i = 0; i < 3; i++)
		executionDoneSemaphores.push_back(renderer->createSemaphore());

	build();
}

void VulkanRenderGraph::cleanupResources()
{
	for (auto it = resourceImageViews.begin(); it != resourceImageViews.end(); it++)
	{
		vkDestroyImageView(static_cast<VulkanRenderer*>(renderer)->device, static_cast<VulkanTextureView*>(it->second)->imageView, nullptr);

		delete it->second;
	}

	for (size_t i = 0; i < graphImages.size(); i++)
		vmaDestroyImage(static_cast<VulkanRenderer*>(renderer)->memAllocator, graphImages[i].imageHandle, graphImages[i].imageMemory);

	for (size_t i = 0; i < beginRenderPassOpsData.size(); i++)
	{
		vkDestroyFramebuffer(static_cast<VulkanRenderer*>(renderer)->device, beginRenderPassOpsData[i]->framebuffer, nullptr);
		vkDestroyRenderPass(static_cast<VulkanRenderer*>(renderer)->device, beginRenderPassOpsData[i]->renderPass, nullptr);
	}

	for (CommandPool pool : gfxCommandPools)
		renderer->destroyCommandPool(pool);

	for (Semaphore sem : executionDoneSemaphores)
		renderer->destroySemaphore(sem);

	graphImages.clear();
	graphImagesMap.clear();
	resourceImageViews.clear();
	allAttachments.clear();
	execCodes.clear();
	beginRenderPassOpsData.clear();
	postBlitOpsData.clear();
	callRenderFuncOpsData.clear();
	gfxCommandPools.clear();
	executionDoneSemaphores.clear();

	attachmentUsageLifetimes.clear();
	attachmentAliasingCandidates.clear();
}

void VulkanRenderGraph::assignPhysicalResources(const std::vector<size_t> &passStack)
{
	std::map<std::string, VkImageUsageFlags> attachmentUsages;

	// Determine image usages
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RendererGraphRenderPass &pass = *passes[passStack[i]];

		for (const std::string &s : pass.getColorInputs())
			attachmentUsages[s] |= VK_IMAGE_USAGE_SAMPLED_BIT;

		for (const std::string &s : pass.getDepthStencilInputs())
			attachmentUsages[s] |= VK_IMAGE_USAGE_SAMPLED_BIT;

		for (const std::string &s : pass.getColorFragmentInputAttachments())
			attachmentUsages[s] |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		for (const std::string &s : pass.getDepthStencilFragmentInputAttachments())
			attachmentUsages[s] |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;

		for (size_t o = 0; o < pass.getColorOutputs().size(); o++)
		{
			const std::string &s = pass.getColorOutputs()[o].first;

			allAttachments.emplace(pass.getColorOutputs()[o]);

			if (pass.getPipelineType() == RG_PIPELINE_GRAPHICS)
				attachmentUsages[s] |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			if (pass.getColorOutputs()[o].second.genMipsWithPostBlit)
				attachmentUsages[s] |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			if (s == renderGraphOutputAttachment)
				attachmentUsages[s] |= VK_IMAGE_USAGE_SAMPLED_BIT;

			if (pass.getPipelineType() == RG_PIPELINE_COMPUTE)
				attachmentUsages[s] |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		}

		if (pass.hasDepthStencilOutput())
		{
			allAttachments.emplace(pass.getDepthStencilOutput());

			const std::string &s = pass.getDepthStencilOutput().first;

			attachmentUsages[s] |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

			if (pass.getDepthStencilOutput().second.genMipsWithPostBlit)
				attachmentUsages[s] |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

			if (pass.getDepthStencilOutput().first == renderGraphOutputAttachment)
				attachmentUsages[s] |= VK_IMAGE_USAGE_SAMPLED_BIT;

			if (pass.getPipelineType() == RG_PIPELINE_COMPUTE)
				attachmentUsages[s] |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		}
	}

	for (auto it = allAttachments.begin(); it != allAttachments.end(); it++)
	{
		uint32_t sizeX = it->second.attachment.namedRelativeSize == "" ? uint32_t(it->second.attachment.sizeX) : uint32_t(namedSizes[it->second.attachment.namedRelativeSize].x * it->second.attachment.sizeX);
		uint32_t sizeY = it->second.attachment.namedRelativeSize == "" ? uint32_t(it->second.attachment.sizeY) : uint32_t(namedSizes[it->second.attachment.namedRelativeSize].y * it->second.attachment.sizeY);

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.extent = {sizeX, sizeY, 1};
		imageCreateInfo.imageType = sizeY > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D;
		imageCreateInfo.mipLevels = it->second.attachment.mipLevels;
		imageCreateInfo.arrayLayers = it->second.attachment.arrayLayers;
		imageCreateInfo.format = toVkFormat(it->second.attachment.format);
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.usage = attachmentUsages[it->first];
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.flags = (it->second.attachment.arrayLayers == 6 ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : 0);

		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
		allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

		VulkanRenderGraphImage graphImage = {};
		graphImage.usageFlags = imageCreateInfo.usage;
		graphImage.attachment = it->second.attachment;

		VK_CHECK_RESULT(vmaCreateImage(static_cast<VulkanRenderer*>(renderer)->memAllocator, &imageCreateInfo, &allocInfo, &graphImage.imageHandle, &graphImage.imageMemory, nullptr));

		graphImages.push_back(graphImage);

		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.image = graphImage.imageHandle;
		imageViewCreateInfo.viewType = toVkImageViewType(it->second.attachment.viewType);
		imageViewCreateInfo.format = imageCreateInfo.format;
		imageViewCreateInfo.subresourceRange.aspectMask = isVkDepthFormat(imageViewCreateInfo.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = it->second.attachment.mipLevels;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = it->second.attachment.arrayLayers;

		VulkanTextureView *resourceImageView = new VulkanTextureView();
		resourceImageView->parentTexture = nullptr;
	
		VK_CHECK_RESULT(vkCreateImageView(static_cast<VulkanRenderer*>(renderer)->device, &imageViewCreateInfo, nullptr, &resourceImageView->imageView));

		resourceImageViews[it->first] = resourceImageView;
		graphImagesMap[it->first] = graphImages.size() - 1;

		for (size_t m = 0; m < it->second.attachment.mipLevels; m++)
		{
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = graphImage.imageHandle;
			imageViewCreateInfo.viewType = toVkImageViewType(it->second.attachment.viewType);
			imageViewCreateInfo.format = imageCreateInfo.format;
			imageViewCreateInfo.subresourceRange.aspectMask = isVkDepthFormat(imageViewCreateInfo.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = m;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = it->second.attachment.arrayLayers;

			resourceImageView = new VulkanTextureView();
			resourceImageView->parentTexture = nullptr;

			VK_CHECK_RESULT(vkCreateImageView(static_cast<VulkanRenderer*>(renderer)->device, &imageViewCreateInfo, nullptr, &resourceImageView->imageView));

			resourceImageViews[it->first + viewSelectMipPostfix + toString(m)] = resourceImageView;
			graphImagesMap[it->first + viewSelectMipPostfix + toString(m)] = graphImages.size() - 1;
		}

		for (size_t a = 0; a < it->second.attachment.arrayLayers; a++)
		{
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.image = graphImage.imageHandle;
			imageViewCreateInfo.viewType = toVkImageViewType(it->second.attachment.viewType);
			imageViewCreateInfo.format = imageCreateInfo.format;
			imageViewCreateInfo.subresourceRange.aspectMask = isVkDepthFormat(imageViewCreateInfo.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = a;
			imageViewCreateInfo.subresourceRange.layerCount = 1;

			resourceImageView = new VulkanTextureView();
			resourceImageView->parentTexture = nullptr;

			VK_CHECK_RESULT(vkCreateImageView(static_cast<VulkanRenderer*>(renderer)->device, &imageViewCreateInfo, nullptr, &resourceImageView->imageView));

			resourceImageViews[it->first + viewSelectLayerPostfix + toString(a)] = resourceImageView;
			graphImagesMap[it->first + viewSelectLayerPostfix + toString(a)] = graphImages.size() - 1;
		}
	}
}

void VulkanRenderGraph::finishBuild(const std::vector<size_t> &passStack)
{
	std::map<std::string, size_t> currentRPAttachmentsMap;
	std::vector<VkAttachmentDescription> currentRPAttachments;
	std::vector<VkClearValue> currentRPAttClearValues;
	std::map<std::string, std::unique_ptr<VkAttachmentReference>> currentRPDepthStencilReferences;
	std::vector<std::unique_ptr<std::vector<VkAttachmentReference>>> currentRPAttachmentRefs;
	std::vector<std::unique_ptr<std::vector<uint32_t>>> currentRPPreserveAttachmentRefs;
	std::vector<VkSubpassDescription> currentRPSubpasses;
	std::vector<VkSubpassDependency> currentRPSubpassDependencies;
	std::vector<size_t> currentRPPassIndices;
	std::vector<size_t> currentRPPassSubpassCount;

	bool buildingRenderPass = false;
	std::vector<std::pair<VulkanRenderGraphOpCode, void*>> currentRPOpCodes;

	for (size_t p = 0; p < passStack.size(); p++)
	{
		RendererGraphRenderPass &pass = *passes[passStack[p]];

		//printf("on pass: %s\n", pass.getNodeName().c_str());

		if (pass.getPipelineType() == RG_PIPELINE_GRAPHICS)
		{
			bool buildingLayersInSubpasses = false, buildingLayersInMultiRenderPasses = false;
			uint32_t renderLayerCount = 1;

			/*
			if (pass.getColorOutputs().size() > 0)
			{
				if (pass.getColorOutputRenderLayerMethod(pass.getColorOutputs()[0].first) == RP_LAYER_RENDER_IN_MULTIPLE_SUBPASSES)
				{
					buildingLayersInSubpasses = true;
					renderLayerCount = pass.getColorOutputs()[0].second.arrayLayers;
				}
				else if (pass.getColorOutputRenderLayerMethod(pass.getColorOutputs()[0].first) == RP_LAYER_RENDER_IN_MULTIPLE_RENDERPASSES)
				{
					buildingLayersInMultiRenderPasses = true;
					renderLayerCount = pass.getColorOutputs()[0].second.arrayLayers;
				}
			}
			*/

			/*
			if (pass.hasDepthStencilOutput())
			{
				if (pass.getColorOutputRenderLayerMethod(pass.getDepthStencilOutput().first) == RP_LAYER_RENDER_IN_MULTIPLE_SUBPASSES)
				{
					buildingLayersInSubpasses = true;
					renderLayerCount = pass.getDepthStencilOutput().second.arrayLayers;
				}
				else if (pass.getColorOutputRenderLayerMethod(pass.getDepthStencilOutput().first) == RP_LAYER_RENDER_IN_MULTIPLE_RENDERPASSES)
				{
					buildingLayersInMultiRenderPasses = true;
					renderLayerCount = pass.getDepthStencilOutput().second.arrayLayers;
				}
			}
			*/

			for (uint32_t l = 0; l < renderLayerCount; l++)
			{
				if (buildingLayersInMultiRenderPasses || l == 0)
				{
					currentRPPassSubpassCount.push_back(buildingLayersInSubpasses ? renderLayerCount : 1);
					currentRPPassIndices.push_back(passStack[p]);
				}

				if (!buildingRenderPass)
				{
					for (size_t i = 0; i < pass.getColorFragmentInputAttachments().size(); i++)
					{
						std::string attName = pass.getColorFragmentInputAttachments()[i];

						VkAttachmentDescription attachmentDesc = {};
						attachmentDesc.format = toVkFormat(allAttachments[attName].attachment.format);
						attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
						attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
						attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

						currentRPAttachmentsMap[attName] = currentRPAttachments.size();
						currentRPAttachments.push_back(attachmentDesc);
						currentRPAttClearValues.push_back({ 0, 0, 0, 0 });
					}

					for (size_t i = 0; i < pass.getDepthStencilFragmentInputAttachments().size(); i++)
					{
						std::string attName = pass.getDepthStencilFragmentInputAttachments()[i];

						VkAttachmentDescription attachmentDesc = {};
						attachmentDesc.format = toVkFormat(allAttachments[attName].attachment.format);
						attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
						attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
						attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
						attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
						attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

						currentRPAttachmentsMap[attName] = currentRPAttachments.size();
						currentRPAttachments.push_back(attachmentDesc);
						currentRPAttClearValues.push_back({ 0, 0, 0, 0 });
					}

					buildingRenderPass = true;
				}
				else
				{
					for (size_t i = 0; i < pass.getColorFragmentInputAttachments().size(); i++)
					{
						std::string attName = pass.getColorFragmentInputAttachments()[i];

						if (currentRPAttachmentsMap.count(attName) == 0)
						{
							VkAttachmentDescription attachmentDesc = {};
							attachmentDesc.format = toVkFormat(allAttachments[attName].attachment.format);
							attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
							attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
							attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
							attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

							currentRPAttachmentsMap[attName] = currentRPAttachments.size();
							currentRPAttachments.push_back(attachmentDesc);
							currentRPAttClearValues.push_back({ 0, 0, 0, 0 });
						}
					}

					for (size_t i = 0; i < pass.getDepthStencilFragmentInputAttachments().size(); i++)
					{
						std::string attName = pass.getDepthStencilFragmentInputAttachments()[i];

						if (currentRPAttachmentsMap.count(attName) == 0)
						{
							VkAttachmentDescription attachmentDesc = {};
							attachmentDesc.format = toVkFormat(allAttachments[attName].attachment.format);
							attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
							attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
							attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
							attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
							attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

							currentRPAttachmentsMap[attName] = currentRPAttachments.size();
							currentRPAttachments.push_back(attachmentDesc);
							currentRPAttClearValues.push_back({ 0, 0, 0, 0 });
						}
					}

					if (!buildingLayersInSubpasses && !buildingLayersInMultiRenderPasses)
						currentRPOpCodes.push_back(std::make_pair<VulkanRenderGraphOpCode, void*>(VKRG_OPCODE_NEXT_SUBPASS, nullptr));
				}

				for (size_t i = 0; i < pass.getColorOutputs().size(); i++)
				{
					std::string attName = pass.getColorOutputs()[i].first;
					const RenderPassOutputAttachment &outAtt = allAttachments[attName];

					VkAttachmentDescription attachmentDesc = {};
					attachmentDesc.format = toVkFormat(outAtt.attachment.format);
					attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					attachmentDesc.loadOp = outAtt.clearAttachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

					currentRPAttachmentsMap[attName + ((buildingLayersInSubpasses || buildingLayersInMultiRenderPasses) ? viewSelectLayerPostfix : viewSelectMipPostfix) + toString(l)] = currentRPAttachments.size();
					currentRPAttachments.push_back(attachmentDesc);
					currentRPAttClearValues.push_back(*reinterpret_cast<const VkClearValue*>(&outAtt.attachmentClearValue));
				}

				if (pass.hasDepthStencilOutput())
				{
					std::string attName = pass.getDepthStencilOutput().first;
					const RenderPassOutputAttachment &outAtt = allAttachments[attName];

					VkAttachmentDescription attachmentDesc = {};
					attachmentDesc.format = toVkFormat(outAtt.attachment.format);
					attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
					attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
					attachmentDesc.loadOp = outAtt.clearAttachment ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
					attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
					attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;

					currentRPAttachmentsMap[attName + ((buildingLayersInSubpasses || buildingLayersInMultiRenderPasses) ? viewSelectLayerPostfix : viewSelectMipPostfix) + toString(l)] = currentRPAttachments.size();
					currentRPAttachments.push_back(attachmentDesc);
					currentRPAttClearValues.push_back(*reinterpret_cast<const VkClearValue*>(&outAtt.attachmentClearValue));
				}

				currentRPAttachmentRefs.push_back(std::unique_ptr<std::vector<VkAttachmentReference>>(new std::vector<VkAttachmentReference>()));
				std::vector<VkAttachmentReference> &subpassInputAttachments = *currentRPAttachmentRefs.back().get();

				currentRPAttachmentRefs.push_back(std::unique_ptr<std::vector<VkAttachmentReference>>(new std::vector<VkAttachmentReference>()));
				std::vector<VkAttachmentReference> &subpassColorAttachments = *currentRPAttachmentRefs.back().get();

				currentRPPreserveAttachmentRefs.push_back(std::unique_ptr<std::vector<uint32_t>>(new std::vector<uint32_t>()));
				std::vector<uint32_t> &subpassPreserveAttachments = *currentRPPreserveAttachmentRefs.back().get();

				for (size_t i = 0; i < pass.getColorFragmentInputAttachments().size(); i++)
				{
					VkAttachmentReference attRef = {};
					attRef.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					attRef.attachment = (uint32_t)currentRPAttachmentsMap[pass.getColorFragmentInputAttachments()[i]];

					subpassInputAttachments.push_back(attRef);
				}

				for (size_t i = 0; i < pass.getDepthStencilFragmentInputAttachments().size(); i++)
				{
					VkAttachmentReference attRef = {};
					attRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
					attRef.attachment = (uint32_t)currentRPAttachmentsMap[pass.getDepthStencilFragmentInputAttachments()[i]];

					subpassInputAttachments.push_back(attRef);
				}

				for (size_t i = 0; i < pass.getColorOutputs().size(); i++)
				{
					VkAttachmentReference attRef = {};
					attRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
					attRef.attachment = (uint32_t)currentRPAttachmentsMap[pass.getColorOutputs()[i].first + ((buildingLayersInSubpasses || buildingLayersInMultiRenderPasses) ? viewSelectLayerPostfix : viewSelectMipPostfix) + toString(l)];

					subpassColorAttachments.push_back(attRef);
				}

				if (pass.hasDepthStencilOutput())
				{
					VkAttachmentReference *attRef = new VkAttachmentReference();
					attRef->layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
					attRef->attachment = (uint32_t)currentRPAttachmentsMap[pass.getDepthStencilOutput().first + ((buildingLayersInSubpasses || buildingLayersInMultiRenderPasses) ? viewSelectLayerPostfix : viewSelectMipPostfix) + toString(l)];

					currentRPDepthStencilReferences[pass.getDepthStencilOutput().first + ((buildingLayersInSubpasses || buildingLayersInMultiRenderPasses) ? viewSelectLayerPostfix : viewSelectMipPostfix) + toString(l)] = std::unique_ptr<VkAttachmentReference>(attRef);
				}

				for (size_t i = 0; i < currentRPAttachments.size(); i++)
				{
					bool found = false;

					for (size_t a = 0; a < subpassInputAttachments.size(); a++)
					{
						if (subpassInputAttachments[a].attachment == i)
						{
							found = true;
							break;
						}
					}

					if (!found)
					{
						for (size_t a = 0; a < subpassColorAttachments.size(); a++)
						{
							if (subpassColorAttachments[a].attachment == i)
							{
								found = true;
								break;
							}
						}
					}

					if (!found && pass.hasDepthStencilOutput() && currentRPDepthStencilReferences[pass.getDepthStencilOutput().first + ((buildingLayersInSubpasses || buildingLayersInMultiRenderPasses) ? viewSelectLayerPostfix : viewSelectMipPostfix) + toString(l)]->attachment == i)
						found = true;

					if (!found)
						subpassPreserveAttachments.push_back(i);
				}

				VkSubpassDescription subpassDesc = {};
				subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
				subpassDesc.inputAttachmentCount = subpassInputAttachments.size();
				subpassDesc.pInputAttachments = subpassInputAttachments.data();
				subpassDesc.colorAttachmentCount = subpassColorAttachments.size();
				subpassDesc.pColorAttachments = subpassColorAttachments.data();
				subpassDesc.pDepthStencilAttachment = pass.hasDepthStencilOutput() ? currentRPDepthStencilReferences[pass.getDepthStencilOutput().first + ((buildingLayersInSubpasses || buildingLayersInMultiRenderPasses) ? viewSelectLayerPostfix : viewSelectMipPostfix) + toString(l)].get() : nullptr;
				subpassDesc.preserveAttachmentCount = subpassPreserveAttachments.size();
				subpassDesc.pPreserveAttachments = subpassPreserveAttachments.data();

				currentRPSubpasses.push_back(subpassDesc);

				callRenderFuncOpsData.push_back(std::unique_ptr<VulkanRenderGraphCallRenderFuncOpData>(new VulkanRenderGraphCallRenderFuncOpData()));
				VulkanRenderGraphCallRenderFuncOpData &crfopData = *callRenderFuncOpsData.back();
				crfopData.passIndex = passStack[p];
				crfopData.counter = buildingLayersInMultiRenderPasses ? l : 0;

				if (l == 0 || buildingLayersInMultiRenderPasses)
					currentRPOpCodes.push_back(std::make_pair<VulkanRenderGraphOpCode, void*>(VKRG_OPCODE_CALL_RENDER_FUNC, &crfopData));

				// If we can't merge with the next pass, then we'll end the render pass
				if ((p == passStack.size() - 1 || !checkIsMergeValid(passStack[p + 1], passStack[p], passes)) && (l == renderLayerCount - 1 || buildingLayersInMultiRenderPasses))
				{
					beginRenderPassOpsData.push_back(std::unique_ptr<VulkanRenderGraphBeginRenderPassData>(new VulkanRenderGraphBeginRenderPassData()));
					VulkanRenderGraphBeginRenderPassData &rpData = *(beginRenderPassOpsData.back().get());
					RenderPassAttachment attExample = pass.hasDepthStencilOutput() ? pass.getDepthStencilOutput().second.attachment : pass.getColorOutputs()[0].second.attachment;

					uint32_t attSizeX = attExample.namedRelativeSize == "" ? uint32_t(attExample.sizeX) : uint32_t(namedSizes[attExample.namedRelativeSize].x * attExample.sizeX);
					uint32_t attSizeY = attExample.namedRelativeSize == "" ? uint32_t(attExample.sizeY) : uint32_t(namedSizes[attExample.namedRelativeSize].y * attExample.sizeY);

					std::vector<TextureView> rpFramebufferTVs(currentRPAttachmentsMap.size());
					std::vector<std::string> testStrs(currentRPAttachmentsMap.size());
					std::vector<VkClearValue> rpClearValues(currentRPAttachmentsMap.size());

					for (auto rpAttIt = currentRPAttachmentsMap.begin(); rpAttIt != currentRPAttachmentsMap.end(); rpAttIt++)
					{
						//printf("%u using %s\n", rpAttIt->second, rpAttIt->first.c_str());
						rpFramebufferTVs[rpAttIt->second] = resourceImageViews[rpAttIt->first];
						testStrs[rpAttIt->second] = rpAttIt->first;
						rpClearValues[rpAttIt->second] = *reinterpret_cast<VkClearValue*>(&currentRPAttClearValues[rpAttIt->second]);
					}

					//printf("\n");

					rpData.size = { attSizeX, attSizeY, attExample.arrayLayers };

					VkRenderPassCreateInfo renderPassCreateInfo = {};
					renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
					renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(currentRPAttachments.size());
					renderPassCreateInfo.pAttachments = currentRPAttachments.data();
					renderPassCreateInfo.subpassCount = static_cast<uint32_t>(currentRPSubpasses.size());
					renderPassCreateInfo.pSubpasses = currentRPSubpasses.data();
					renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(currentRPSubpassDependencies.size());
					renderPassCreateInfo.pDependencies = currentRPSubpassDependencies.data();

					VK_CHECK_RESULT(vkCreateRenderPass(static_cast<VulkanRenderer*>(renderer)->device, &renderPassCreateInfo, nullptr, &rpData.renderPass));

					std::vector<VkImageView> rpFramebufferVkTVs;

					for (TextureView tv : rpFramebufferTVs)
						rpFramebufferVkTVs.push_back(static_cast<VulkanTextureView*>(tv)->imageView);

					VkFramebufferCreateInfo framebufferCreateInfo = {};
					framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
					framebufferCreateInfo.renderPass = rpData.renderPass;
					framebufferCreateInfo.attachmentCount = rpFramebufferVkTVs.size();
					framebufferCreateInfo.pAttachments = rpFramebufferVkTVs.data();
					framebufferCreateInfo.width = attSizeX;
					framebufferCreateInfo.height = attSizeY;
					framebufferCreateInfo.layers = attExample.arrayLayers;

					VulkanFramebuffer *framebuffer = new VulkanFramebuffer();

					VK_CHECK_RESULT(vkCreateFramebuffer(static_cast<VulkanRenderer*>(renderer)->device, &framebufferCreateInfo, nullptr, &rpData.framebuffer));

					rpData.subpassContents = VK_SUBPASS_CONTENTS_INLINE;
					rpData.clearValues = rpClearValues;

					uint32_t initBaseSubpass = 0;
					for (size_t i = 0; i < currentRPPassIndices.size(); i++)
					{
						std::unique_ptr<VulkanRenderPass> renderPassContainer(new VulkanRenderPass());
						renderPassContainer->renderPassHandle = rpData.renderPass;

						RenderGraphInitFunctionData initData = {};
						initData.renderPassHandle = renderPassContainer.get();
						initData.baseSubpass = initBaseSubpass;

						passes[currentRPPassIndices[i]]->getInitFunction()(initData);
						initBaseSubpass += currentRPPassSubpassCount[i];
					}

					buildingRenderPass = false;
					currentRPAttachmentsMap.clear();
					currentRPAttachments.clear();
					currentRPDepthStencilReferences.clear();
					currentRPSubpasses.clear();
					currentRPSubpassDependencies.clear();
					currentRPAttClearValues.clear();
					currentRPPassIndices.clear();
					currentRPPassSubpassCount.clear();
					currentRPAttachmentRefs.clear();
					currentRPPreserveAttachmentRefs.clear();

					execCodes.push_back(std::make_pair<VulkanRenderGraphOpCode, void*>(VKRG_OPCODE_BEGIN_RENDER_PASS, &rpData));
					execCodes.insert(execCodes.end(), currentRPOpCodes.begin(), currentRPOpCodes.end());
					execCodes.push_back(std::make_pair<VulkanRenderGraphOpCode, void*>(VKRG_OPCODE_END_RENDER_PASS, nullptr));

					for (size_t i = 0; i < pass.getColorOutputs().size(); i++)
					{
						if (pass.getColorOutputs()[i].second.genMipsWithPostBlit)
						{
							postBlitOpsData.push_back(std::unique_ptr<VulkanRenderGraphPostBlitOpData>(new VulkanRenderGraphPostBlitOpData()));
							VulkanRenderGraphPostBlitOpData &opData = *(postBlitOpsData.back());
							opData.graphImageIndex = graphImagesMap[pass.getColorOutputs()[i].first];

							execCodes.push_back(std::make_pair<VulkanRenderGraphOpCode, void*>(VKRG_OPCODE_POST_BLIT, &opData));
						}
					}

					currentRPOpCodes.clear();
				}
				else
				{
					VkSubpassDependency subpassDep = {};
					subpassDep.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
					subpassDep.dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
					subpassDep.srcSubpass = (uint32_t)currentRPSubpasses.size() - 1;
					subpassDep.dstSubpass = (uint32_t)currentRPSubpasses.size();
					subpassDep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
					subpassDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
					subpassDep.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

					currentRPSubpassDependencies.push_back(subpassDep);
				}
			}
		}
		else if (pass.getPipelineType() == RG_PIPELINE_COMPUTE)
		{
			callRenderFuncOpsData.push_back(std::unique_ptr<VulkanRenderGraphCallRenderFuncOpData>(new VulkanRenderGraphCallRenderFuncOpData()));
			VulkanRenderGraphCallRenderFuncOpData &crfopData = *callRenderFuncOpsData.back();
			crfopData.passIndex = passStack[p];

			execCodes.push_back(std::make_pair<VulkanRenderGraphOpCode, void*>(VKRG_OPCODE_CALL_RENDER_FUNC, &crfopData));

			RenderGraphInitFunctionData initData = {};
			initData.baseSubpass = ~0u;
			initData.renderPassHandle = nullptr;

			passes[passStack[p]]->getInitFunction()(initData);
		}
	}

	for (size_t i = 0; i < execCodes.size(); i++)
	{
		switch (execCodes[i].first)
		{
		case VKRG_OPCODE_BEGIN_RENDER_PASS:
			//printf("VKRG_OPCODE_BEGIN_RENDER_PASS\n");
			break;
		case VKRG_OPCODE_END_RENDER_PASS:
			//printf("VKRG_OPCODE_END_RENDER_PASS\n");
			break;
		case VKRG_OPCODE_NEXT_SUBPASS:
			//printf("VKRG_OPCODE_NEXT_SUBPASS\n");
			break;
		case VKRG_OPCODE_POST_BLIT:
			//printf("VKRG_OPCODE_POST_BLIT\n");
			break;
		case VKRG_OPCODE_CALL_RENDER_FUNC:
			//printf("VKRG_OPCODE_CALL_RENDER_FUNC \"%s\"\n", passes[static_cast<VulkanRenderGraphCallRenderFuncOpData*>(execCodes[i].second)->passIndex]->getNodeName().c_str());
			break;
		}
	}

	RenderGraphDescriptorUpdateFunctionData descUpdateFuncData = {};
	descUpdateFuncData.graphTextureViews = resourceImageViews;

	for (size_t p = 0; p < passStack.size(); p++)
	{
		RendererGraphRenderPass &pass = *passes[passStack[p]];

		pass.getDescriptorUpdateFunction()(descUpdateFuncData);
	}
}

#endif