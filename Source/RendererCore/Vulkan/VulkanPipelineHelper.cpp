#include "RendererCore/Vulkan/VulkanPipelineHelper.h"
#if BUILD_VULKAN_BACKEND

#include <RendererCore/Vulkan/VulkanRenderer.h>
#include <RendererCore/Vulkan/VulkanEnums.h>

VulkanPipelineHelper::VulkanPipelineHelper (VulkanRenderer *parentVulkanRenderer)
{
	renderer = parentVulkanRenderer;
}

VulkanPipelineHelper::~VulkanPipelineHelper ()
{
	for (auto descriptorSetLayout : descriptorSetLayoutCache)
		vkDestroyDescriptorSetLayout(renderer->device, descriptorSetLayout.setLayoutHandle, nullptr);

	for (auto &staticSampler : staticSamplerCache)
		vkDestroySampler(renderer->device, staticSampler.samplerHandle, nullptr);
}

Pipeline VulkanPipelineHelper::createGraphicsPipeline (const GraphicsPipelineInfo &pipelineInfo, RenderPass renderPass, uint32_t subpass)
{
	VulkanPipeline *vulkanPipeline = new VulkanPipeline();
	vulkanPipeline->gfxPipelineInfo = pipelineInfo;

	if (pipelineInfo.inputPushConstants.size > 128)
	{
		Log::get()->error("VulkanPipelineHelper: Cannot create a pipeline with more than 128 bytes of push constants");

		throw std::runtime_error("vulkan error, inputPushConstants.size > 128");
	}

	// Note that only the create infos that don't rely on pointers have been separated into other functions for clarity
	// Many of the vulkan structures do C-style arrays, and so we have to be careful about pointers

	std::vector<VkPipelineShaderStageCreateInfo> pipelineShaderStages = getPipelineShaderStages(pipelineInfo.stages);
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = getPipelineInputAssemblyInfo(pipelineInfo.inputAssemblyInfo);
	VkPipelineRasterizationStateCreateInfo rastState = getPipelineRasterizationInfo(pipelineInfo.rasterizationInfo);
	VkPipelineDepthStencilStateCreateInfo depthStencilState = getPipelineDepthStencilInfo(pipelineInfo.depthStencilInfo);

	// These are the create infos that rely on pointers in their data structs
	std::vector<VkVertexInputBindingDescription> inputBindings;
	std::vector<VkVertexInputAttributeDescription> inputAttribs;

	VkPipelineVertexInputStateCreateInfo vertexInputState = {};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	{
		for (size_t i = 0; i < pipelineInfo.vertexInputInfo.vertexInputBindings.size(); i ++)
		{
			const VertexInputBinding &genericBinding = pipelineInfo.vertexInputInfo.vertexInputBindings[i];
			VkVertexInputBindingDescription binding = {};
			binding.binding = genericBinding.binding;
			binding.stride = genericBinding.stride;
			binding.inputRate = toVkVertexInputRate(genericBinding.inputRate);

			inputBindings.push_back(binding);
		}

		for (size_t i = 0; i < pipelineInfo.vertexInputInfo.vertexInputAttribs.size(); i ++)
		{
			const VertexInputAttribute &genericAttrib = pipelineInfo.vertexInputInfo.vertexInputAttribs[i];
			VkVertexInputAttributeDescription attrib = {};
			attrib.location = genericAttrib.location;
			attrib.binding = genericAttrib.binding;
			attrib.format = ResourceFormatToVkFormat(genericAttrib.format);
			attrib.offset = genericAttrib.offset;

			inputAttribs.push_back(attrib);
		}

		vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(inputBindings.size());
		vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(inputAttribs.size());
		vertexInputState.pVertexBindingDescriptions = inputBindings.data();
		vertexInputState.pVertexAttributeDescriptions = inputAttribs.data();
	}

	std::vector<VkViewport> dummyViewports = {{0.0f, 0.0f, 512.0f, 512.0f, 0.0f, 1.0f}};
	std::vector<VkRect2D> dummyScissors = {{{0, 0}, {512, 512}}};

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;
	viewportState.viewportCount = static_cast<uint32_t>(dummyViewports.size());
	viewportState.pViewports = dummyViewports.data();
	viewportState.scissorCount = static_cast<uint32_t>(dummyScissors.size());
	viewportState.pScissors = dummyScissors.data();

	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.pNext = nullptr;
	multisampleState.flags = 0;
	multisampleState.rasterizationSamples = sampleCountToVkSampleCount(pipelineInfo.rasterizationInfo.multiSampleCount);
	multisampleState.sampleShadingEnable = VK_FALSE;
	multisampleState.minSampleShading = 0;
	multisampleState.pSampleMask = nullptr;
	multisampleState.alphaToCoverageEnable = VK_FALSE;
	multisampleState.alphaToOneEnable = VK_FALSE;

	std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;

	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

	{
		colorBlendState.logicOpEnable = static_cast<VkBool32>(pipelineInfo.colorBlendInfo.logicOpEnable);
		colorBlendState.logicOp = toVkLogicOp(pipelineInfo.colorBlendInfo.logicOp);
		colorBlendState.blendConstants[0] = pipelineInfo.colorBlendInfo.blendConstants[0];
		colorBlendState.blendConstants[1] = pipelineInfo.colorBlendInfo.blendConstants[1];
		colorBlendState.blendConstants[2] = pipelineInfo.colorBlendInfo.blendConstants[2];
		colorBlendState.blendConstants[3] = pipelineInfo.colorBlendInfo.blendConstants[3];

		for (size_t i = 0; i < pipelineInfo.colorBlendInfo.attachments.size(); i ++)
		{
			const PipelineColorBlendAttachment &genericAttachment = pipelineInfo.colorBlendInfo.attachments[i];
			VkPipelineColorBlendAttachmentState attachment = {};
			attachment.blendEnable = static_cast<VkBool32>(genericAttachment.blendEnable);
			attachment.srcColorBlendFactor = toVkBlendFactor(genericAttachment.srcColorBlendFactor);
			attachment.dstColorBlendFactor = toVkBlendFactor(genericAttachment.dstColorBlendFactor);
			attachment.colorBlendOp = toVkBlendOp(genericAttachment.colorBlendOp);
			attachment.srcAlphaBlendFactor = toVkBlendFactor(genericAttachment.srcAlphaBlendFactor);
			attachment.dstAlphaBlendFactor = toVkBlendFactor(genericAttachment.dstAlphaBlendFactor);
			attachment.alphaBlendOp = toVkBlendOp(genericAttachment.alphaBlendOp);
			attachment.colorWriteMask = static_cast<VkColorComponentFlags>(genericAttachment.colorWriteMask);

			colorBlendAttachments.push_back(attachment);
		}

		colorBlendState.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
		colorBlendState.pAttachments = colorBlendAttachments.data();
	}

	VkPipelineTessellationStateCreateInfo tessellationState = {};
	tessellationState.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
	tessellationState.pNext = nullptr;
	tessellationState.patchControlPoints = pipelineInfo.tessellationPatchControlPoints;
	tessellationState.flags = 0;

	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();
	
	VkPipelineRasterizationStateRasterizationOrderAMD rastOrderAMD = {};

	if (VulkanExtensions::enabled_VK_AMD_rasterization_order)
	{
		rastOrderAMD.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_RASTERIZATION_ORDER_AMD;
		rastOrderAMD.rasterizationOrder = pipelineInfo.rasterizationInfo.enableOutOfOrderRasterization ? VK_RASTERIZATION_ORDER_RELAXED_AMD : VK_RASTERIZATION_ORDER_STRICT_AMD;
		rastState.pNext = &rastOrderAMD;
	}

	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = static_cast<uint32_t>(pipelineShaderStages.size());
	pipelineCreateInfo.pStages = pipelineShaderStages.data();
	pipelineCreateInfo.pVertexInputState = &vertexInputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pTessellationState = &tessellationState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pRasterizationState = &rastState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pDynamicState = dynamicStates.size() == 0 ? nullptr : &dynamicState;
	pipelineCreateInfo.renderPass = static_cast<VulkanRenderPass*>(renderPass)->renderPassHandle;
	pipelineCreateInfo.subpass = subpass;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;

	std::vector<VkPushConstantRange> vulkanPushConstantRanges;
	std::vector<VkDescriptorSetLayout> vulkanDescSetLayouts;

	if (pipelineInfo.inputPushConstants.size > 0)
	{
		const PushConstantRange &genericPushRange = pipelineInfo.inputPushConstants;
		VkPushConstantRange vulkanPushRange = {};
		vulkanPushRange.stageFlags = genericPushRange.stageAccessFlags;
		vulkanPushRange.size = genericPushRange.size;
		vulkanPushRange.offset = 0;

		vulkanPushConstantRanges.push_back(vulkanPushRange);
	}

	for (size_t i = 0; i < pipelineInfo.inputSetLayouts.size(); i ++)
	{
		vulkanDescSetLayouts.push_back(createDescriptorSetLayout(pipelineInfo.inputSetLayouts[i]));
	}

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(vulkanPushConstantRanges.size());
	layoutCreateInfo.pPushConstantRanges = vulkanPushConstantRanges.data();
	layoutCreateInfo.setLayoutCount = static_cast<uint32_t>(pipelineInfo.inputSetLayouts.size());
	layoutCreateInfo.pSetLayouts = vulkanDescSetLayouts.data();

	VK_CHECK_RESULT(vkCreatePipelineLayout(renderer->device, &layoutCreateInfo, nullptr, &vulkanPipeline->pipelineLayoutHandle));

	pipelineCreateInfo.layout = vulkanPipeline->pipelineLayoutHandle;

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &vulkanPipeline->pipelineHandle));

	return vulkanPipeline;
}

Pipeline VulkanPipelineHelper::createComputePipeline(const ComputePipelineInfo &pipelineInfo)
{
	VulkanPipeline *vulkanPipeline = new VulkanPipeline();
	vulkanPipeline->computePipelineInfo = pipelineInfo;

	VkComputePipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.flags = 0;

	VkPipelineShaderStageCreateInfo computeShaderStage = {};
	computeShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStage.flags = 0;
	computeShaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStage.module = static_cast<VulkanShaderModule*>(pipelineInfo.shader.shaderModule)->module;
	computeShaderStage.pName = pipelineInfo.shader.shaderModule->entryPoint.c_str();
	computeShaderStage.pSpecializationInfo = nullptr;

	pipelineCreateInfo.stage = computeShaderStage;

	std::vector<VkPushConstantRange> vulkanPushConstantRanges;
	std::vector<VkDescriptorSetLayout> vulkanDescSetLayouts;

	if (pipelineInfo.inputPushConstants.size > 0)
	{
		const PushConstantRange &genericPushRange = pipelineInfo.inputPushConstants;
		VkPushConstantRange vulkanPushRange = {};
		vulkanPushRange.stageFlags = genericPushRange.stageAccessFlags;
		vulkanPushRange.size = genericPushRange.size;
		vulkanPushRange.offset = 0;

		vulkanPushConstantRanges.push_back(vulkanPushRange);
	}

	for (size_t i = 0; i < pipelineInfo.inputSetLayouts.size(); i++)
	{
		vulkanDescSetLayouts.push_back(createDescriptorSetLayout(pipelineInfo.inputSetLayouts[i]));
	}

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(vulkanPushConstantRanges.size());
	layoutCreateInfo.pPushConstantRanges = vulkanPushConstantRanges.data();
	layoutCreateInfo.setLayoutCount = static_cast<uint32_t>(pipelineInfo.inputSetLayouts.size());
	layoutCreateInfo.pSetLayouts = vulkanDescSetLayouts.data();

	VK_CHECK_RESULT(vkCreatePipelineLayout(renderer->device, &layoutCreateInfo, nullptr, &vulkanPipeline->pipelineLayoutHandle));

	pipelineCreateInfo.layout = vulkanPipeline->pipelineLayoutHandle;

	VK_CHECK_RESULT(vkCreateComputePipelines(renderer->device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &vulkanPipeline->pipelineHandle));

	return vulkanPipeline;
}

VkDescriptorSetLayout VulkanPipelineHelper::createDescriptorSetLayout (const DescriptorSetLayoutDescription &setDescription)
{
	// Search the cache and see if we have an existing descriptor set layout to use
	for (size_t i = 0; i < descriptorSetLayoutCache.size(); i++)
		if (descriptorSetLayoutCache[i].description == setDescription)
			return descriptorSetLayoutCache[i].setLayoutHandle;

	std::vector<VkDescriptorSetLayoutBinding> bindings;
	std::vector<VkSampler> staticSamplerHandles;
	
	size_t staticSamplerCount = 0;

	for (size_t i = 0; i < setDescription.bindings.size(); i++)
		if (setDescription.bindings[i].type == DESCRIPTOR_TYPE_SAMPLER)
			staticSamplerCount += setDescription.bindings[i].staticSamplers.size();

	staticSamplerHandles.reserve(staticSamplerCount);

	for (size_t i = 0; i < setDescription.bindings.size(); i++)
	{
		const DescriptorSetBinding &binding = setDescription.bindings[i];
		VkDescriptorSetLayoutBinding vulkanBinding = {};
		vulkanBinding.binding = binding.binding;
		vulkanBinding.descriptorCount = binding.arrayCount;
		vulkanBinding.stageFlags = toVkShaderStageFlags(binding.stageAccessMask);
		vulkanBinding.descriptorType = toVkDescriptorType(binding.type);

		if (binding.type == DESCRIPTOR_TYPE_SAMPLER)
		{
			if (binding.staticSamplers.size() > 0)
			{
				if (binding.staticSamplers.size() != binding.arrayCount)
				{
					Log::get()->error("VulkanPipelineHelper: If a sampler binding has any number of static samplers ALL samplers in that binding must be static! (AKA staticSamplers.size() == binding.arrayCount)");
					throw std::runtime_error("vulkan - static sampler count doesn't match sampler count in descriptor set binding");
				}

				for (size_t s = 0; s < binding.staticSamplers.size(); s++)
				{
					size_t cachedStaticSamplerIndex = std::numeric_limits<size_t>::max();
					const DescriptorStaticSampler &samplerInfo = binding.staticSamplers[s];

					for (size_t cs = 0; cs < staticSamplerCache.size(); cs++)
					{
						if (staticSamplerCache[cs].samplerDesc == samplerInfo)
						{
							cachedStaticSamplerIndex = cs;
							break;
						}
					}

					if (cachedStaticSamplerIndex != std::numeric_limits<size_t>::max())
					{
						staticSamplerHandles.push_back(staticSamplerCache[cachedStaticSamplerIndex].samplerHandle);
					}
					else
					{
						VkSamplerCreateInfo samplerCreateInfo = {};
						samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
						samplerCreateInfo.pNext = nullptr;
						samplerCreateInfo.flags = 0;
						samplerCreateInfo.minFilter = toVkFilter(samplerInfo.minFilter);
						samplerCreateInfo.magFilter = toVkFilter(samplerInfo.magFilter);
						samplerCreateInfo.addressModeU = toVkSamplerAddressMode(samplerInfo.addressMode);
						samplerCreateInfo.addressModeV = toVkSamplerAddressMode(samplerInfo.addressMode);
						samplerCreateInfo.addressModeW = toVkSamplerAddressMode(samplerInfo.addressMode);
						samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
						samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
						samplerCreateInfo.compareEnable = VK_FALSE;
						samplerCreateInfo.mipmapMode = toVkSamplerMipmapMode(samplerInfo.mipmapMode);
						samplerCreateInfo.mipLodBias = samplerInfo.mipLodBias;
						samplerCreateInfo.minLod = samplerInfo.minLod;
						samplerCreateInfo.maxLod = samplerInfo.maxLod;

						if (renderer->deviceFeatures.samplerAnisotropy && samplerInfo.maxAnisotropy > 1.0f)
						{
							samplerCreateInfo.anisotropyEnable = VK_TRUE;
							samplerCreateInfo.maxAnisotropy = std::min<float>(samplerInfo.maxAnisotropy, renderer->deviceProps.limits.maxSamplerAnisotropy);
						}
						else
						{
							samplerCreateInfo.anisotropyEnable = VK_FALSE;
							samplerCreateInfo.maxAnisotropy = 1.0f;
						}

						VkSampler samplerHandle;
						VK_CHECK_RESULT(vkCreateSampler(renderer->device, &samplerCreateInfo, nullptr, &samplerHandle));

						staticSamplerHandles.push_back(samplerHandle);

						VulkanDescriptorSetStaticSamplerCache newCachedSampler = {};
						newCachedSampler.samplerDesc = samplerInfo;
						newCachedSampler.samplerHandle = samplerHandle;

						staticSamplerCache.push_back(newCachedSampler);
					}
				}

				vulkanBinding.pImmutableSamplers = &staticSamplerHandles[staticSamplerHandles.size() - binding.arrayCount];
			}
		}

		bindings.push_back(vulkanBinding);
	}

	VkDescriptorSetLayoutCreateInfo setLayoutCreateInfo = {};
	setLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCreateInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	setLayoutCreateInfo.pBindings = bindings.data();

	VkDescriptorSetLayout setLayout;

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(renderer->device, &setLayoutCreateInfo, nullptr, &setLayout));

	VulkanDescriptorSetLayoutCacheInfo setLayoutCacheInfo = {};
	setLayoutCacheInfo.description = setDescription;
	setLayoutCacheInfo.setLayoutHandle = setLayout;

	descriptorSetLayoutCache.push_back(setLayoutCacheInfo);

	return setLayout;
}

std::vector<VkPipelineShaderStageCreateInfo> VulkanPipelineHelper::getPipelineShaderStages (const std::vector<PipelineShaderStage> &stages)
{
	std::vector<VkPipelineShaderStageCreateInfo> vulkanStageInfo;

	for (size_t i = 0; i < stages.size(); i ++)
	{
		const VulkanShaderModule *shaderModule = static_cast<VulkanShaderModule*>(stages[i].shaderModule);
		VkPipelineShaderStageCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		createInfo.pName = stages[i].shaderModule->entryPoint.c_str();
		createInfo.module = shaderModule->module;
		createInfo.stage = toVkShaderStageFlagBits(shaderModule->stage);

		vulkanStageInfo.push_back(createInfo);
	}

	return vulkanStageInfo;
}

VkPipelineInputAssemblyStateCreateInfo VulkanPipelineHelper::getPipelineInputAssemblyInfo (const PipelineInputAssemblyInfo &info)
{
	VkPipelineInputAssemblyStateCreateInfo inputCreateInfo = {};
	inputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputCreateInfo.primitiveRestartEnable = static_cast<VkBool32>(info.primitiveRestart);
	inputCreateInfo.topology = toVkPrimitiveTopology(info.topology);

	return inputCreateInfo;
}

VkPipelineRasterizationStateCreateInfo VulkanPipelineHelper::getPipelineRasterizationInfo (const PipelineRasterizationInfo &info)
{
	VkPipelineRasterizationStateCreateInfo rastCreateInfo = {};
	rastCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rastCreateInfo.depthClampEnable = VK_FALSE;
	rastCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rastCreateInfo.polygonMode = toVkPolygonMode(info.polygonMode);
	rastCreateInfo.cullMode = PolygonCullModetoVkCullModeFlags(info.cullMode);
	rastCreateInfo.frontFace = info.clockwiseFrontFace ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
	// The depth bias info will go here when (if) I implement it
	rastCreateInfo.lineWidth = 1;

	return rastCreateInfo;
}

VkPipelineDepthStencilStateCreateInfo VulkanPipelineHelper::getPipelineDepthStencilInfo (const PipelineDepthStencilInfo &info)
{
	VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo = {};
	depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilCreateInfo.depthTestEnable = static_cast<VkBool32>(info.enableDepthTest);
	depthStencilCreateInfo.depthWriteEnable = static_cast<VkBool32>(info.enableDepthWrite);
	depthStencilCreateInfo.depthCompareOp = toVkCompareOp(info.depthCompareOp);
	depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
	// The stencil stuff will go here when (if) I implement it
	depthStencilCreateInfo.minDepthBounds = 0.0f;
	depthStencilCreateInfo.maxDepthBounds = 1.0f;

	return depthStencilCreateInfo;
}

#endif
