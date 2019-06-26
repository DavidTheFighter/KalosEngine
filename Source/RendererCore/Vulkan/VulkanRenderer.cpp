#include "RendererCore/Vulkan/VulkanRenderer.h"
#if BUILD_VULKAN_BACKEND

#include <Peripherals/Window.h>
#include <RendererCore/Vulkan/VulkanSwapchain.h>
#include <RendererCore/Vulkan/VulkanPipelineHelper.h>
#include <RendererCore/Vulkan/VulkanEnums.h>
#include <RendererCore/Vulkan/VulkanShaderLoader.h>
#include <RendererCore/Vulkan/VulkanRenderGraph.h>

#include <GLFW/glfw3.h>

const std::vector<const char*> validationLayers = {"VK_LAYER_LUNARG_standard_validation"};
const std::vector<const char*> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#if defined(SE_DEBUG_BUILD)

#endif

VulkanRenderer::VulkanRenderer (const RendererAllocInfo& allocInfo)
{
	onAllocInfo = allocInfo;
	validationLayersEnabled = false;

	physicalDevice = VK_NULL_HANDLE;

	bool shouldTryEnableValidationLayers = false;

	if (std::find(allocInfo.launchArgs.begin(), allocInfo.launchArgs.end(), "-enable_vulkan_layers") != allocInfo.launchArgs.end())
	{
		shouldTryEnableValidationLayers = true;
	}

#ifdef SE_DEBUG_BUILD
	shouldTryEnableValidationLayers = true;
#endif

	if (shouldTryEnableValidationLayers)
	{
		if (checkValidationLayerSupport(validationLayers))
		{
			validationLayersEnabled = true;

			std::string infoStr = "";

			for (size_t i = 0; i < validationLayers.size(); i ++)
			{
				infoStr += validationLayers[i];

				if (i < validationLayers.size() - 1)
					infoStr += ", ";
			}

			Log::get()->info("Enabling the following validation layers for the vulkan backend: {}", infoStr);
		}
		else
		{
			std::string warnStr = "";

			for (size_t i = 0; i < validationLayers.size(); i ++)
			{
				warnStr += validationLayers[i];

				if (i < validationLayers.size() - 1)
					warnStr += ", ";
			}

			Log::get()->warn("Failed to acquire some validation layers requested for the vulkan backend: {}", warnStr);
		}
	}

	PFN_vkEnumerateInstanceVersion enumerateInstanceVersionFunc = (PFN_vkEnumerateInstanceVersion) vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion");

	uint32_t instanceVersion = VK_API_VERSION_1_0;

	if (enumerateInstanceVersionFunc != nullptr)
	{
		enumerateInstanceVersionFunc(&instanceVersion);
	}

	const uint32_t vulkanVersionMajor = VK_VERSION_MAJOR(instanceVersion);
	const uint32_t vulkanVersionMinor = VK_VERSION_MINOR(instanceVersion);
	const uint32_t vulkanVersionPatch = VK_VERSION_PATCH(instanceVersion);

	if (vulkanVersionMajor < 1 || vulkanVersionMinor < 1)
	{
		Log::get()->error("Cannot create a Vulkan renderer, driver reports version as {}.{}.{}, must be at least 1.1.0", vulkanVersionMajor, vulkanVersionMinor, vulkanVersionPatch);

		throw std::exception("cannot load vulkan, insufficient driver vulkan version");
	}

	Log::get()->info("Vulkan version reported as {}.{}.{}", vulkanVersionMajor, vulkanVersionMinor, vulkanVersionPatch);


	std::vector<const char*> instanceExtensions = getInstanceExtensions();

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = APP_NAME;
	appInfo.pEngineName = ENGINE_NAME;
	appInfo.applicationVersion = VK_MAKE_VERSION(APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_REVISION);
	appInfo.engineVersion = VK_MAKE_VERSION(ENGINE_VERSION_MAJOR, ENGINE_VERSION_MINOR, ENGINE_VERSION_REVISION);
	appInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo instCreateInfo = {};
	instCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instCreateInfo.pApplicationInfo = &appInfo;
	instCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();

	if (validationLayersEnabled)
	{
		instCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		instCreateInfo.ppEnabledLayerNames = validationLayers.data();
	}

	VK_CHECK_RESULT(vkCreateInstance(&instCreateInfo, nullptr, &instance));

	if (validationLayersEnabled)
	{
		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;

		VK_CHECK_RESULT(CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &dbgCallback));
	}

	// Note the swapchain renderer is initialized here because choosePhysicalDevices() relies on querying swapchain support
	swapchains = new VulkanSwapchain(this);
	pipelineHandler = new VulkanPipelineHelper(this);

	choosePhysicalDevice();
	createLogicalDevice();

	VmaAllocatorCreateInfo allocCreateInfo = {};
	allocCreateInfo.physicalDevice = physicalDevice;
	allocCreateInfo.device = device;

	VK_CHECK_RESULT(vmaCreateAllocator(&allocCreateInfo, &memAllocator));

#ifdef __linux__
	defaultCompiler = new shaderc::Compiler();
#endif

	swapchains->init();
	initSwapchain(onAllocInfo.mainWindow);
}

VulkanRenderer::~VulkanRenderer ()
{
	cleanupVulkan();
}

void VulkanRenderer::cleanupVulkan ()
{
	delete swapchains;
	delete pipelineHandler;

	vmaDestroyAllocator(memAllocator);
	vkDestroyDevice(device, nullptr);

	DestroyDebugReportCallbackEXT(instance, dbgCallback, nullptr);

	vkDestroyInstance(instance, nullptr);
}

CommandPool VulkanRenderer::createCommandPool (QueueType queue, CommandPoolFlags flags)
{
	VkCommandPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.flags = toVkCommandPoolCreateFlags(flags);

	switch (queue)
	{
		case QUEUE_TYPE_GRAPHICS:
		{
			poolCreateInfo.queueFamilyIndex = deviceQueueInfo.graphicsFamily;
			break;
		}
		case QUEUE_TYPE_COMPUTE:
		{
			poolCreateInfo.queueFamilyIndex = deviceQueueInfo.computeFamily;
			break;
		}
		case QUEUE_TYPE_TRANSFER:
		{
			poolCreateInfo.queueFamilyIndex = deviceQueueInfo.transferFamily;
			break;
		}
		default:
			poolCreateInfo.queueFamilyIndex = std::numeric_limits<uint32_t>::max();
	}

	VulkanCommandPool *vkCmdPool = new VulkanCommandPool();
	vkCmdPool->queue = queue;
	vkCmdPool->flags = flags;
	vkCmdPool->device = device;

	VK_CHECK_RESULT(vkCreateCommandPool(device, &poolCreateInfo, nullptr, &vkCmdPool->poolHandle));

	return vkCmdPool;
}

void VulkanRenderer::submitToQueue (QueueType queue, const std::vector<CommandBuffer> &cmdBuffers, const std::vector<Semaphore> &waitSemaphores, const std::vector<PipelineStageFlags> &waitSemaphoreStages, const std::vector<Semaphore> &signalSemaphores, Fence fence)
{
	DEBUG_ASSERT(cmdBuffers.size() > 0);
	DEBUG_ASSERT(waitSemaphores.size() == waitSemaphoreStages.size());

	std::vector<VkCommandBuffer> vkCmdBuffers(cmdBuffers.size());

	std::vector<VkSemaphore> vulkanWaitSemaphores(waitSemaphores.size()), vulkanSignalSemaphores(signalSemaphores.size());
	std::vector<VkPipelineStageFlags> vulkanWaitStages(waitSemaphores.size());

	for (size_t i = 0; i < cmdBuffers.size(); i ++)
	{
		vkCmdBuffers[i] = (static_cast<VulkanCommandBuffer*>(cmdBuffers[i])->bufferHandle);
	}

	for (size_t i = 0; i < waitSemaphores.size(); i ++)
	{
		vulkanWaitSemaphores[i] = static_cast<VulkanSemaphore*>(waitSemaphores[i])->semHandle;
		vulkanWaitStages[i] = toVkPipelineStageFlags(waitSemaphoreStages[i]);
	}

	for (size_t i = 0; i < signalSemaphores.size(); i ++)
	{
		vulkanSignalSemaphores[i] = static_cast<VulkanSemaphore*>(signalSemaphores[i])->semHandle;
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = static_cast<uint32_t>(cmdBuffers.size());
	submitInfo.pCommandBuffers = vkCmdBuffers.data();
	submitInfo.waitSemaphoreCount = static_cast<uint32_t>(vulkanWaitSemaphores.size());
	submitInfo.pWaitSemaphores = vulkanWaitSemaphores.data();
	submitInfo.pWaitDstStageMask = vulkanWaitStages.data();
	submitInfo.signalSemaphoreCount = static_cast<uint32_t>(vulkanSignalSemaphores.size());
	submitInfo.pSignalSemaphores = vulkanSignalSemaphores.data();

	VkQueue vulkanQueue;

	switch (queue)
	{
		case QUEUE_TYPE_GRAPHICS:
			vulkanQueue = graphicsQueue;
			break;
		case QUEUE_TYPE_COMPUTE:
			vulkanQueue = computeQueue;
			break;
		case QUEUE_TYPE_TRANSFER:
			vulkanQueue = transferQueue;
			break;
		default:
			vulkanQueue = VK_NULL_HANDLE;
	}

	VK_CHECK_RESULT(vkQueueSubmit(vulkanQueue, 1, &submitInfo, (fence != nullptr) ? (static_cast<VulkanFence*> (fence)->fenceHandle) : (VK_NULL_HANDLE)));
}

void VulkanRenderer::waitForQueueIdle (QueueType queue)
{
	switch (queue)
	{
		case QUEUE_TYPE_GRAPHICS:
			VK_CHECK_RESULT(vkQueueWaitIdle(graphicsQueue))
			;
			break;
		case QUEUE_TYPE_COMPUTE:
			VK_CHECK_RESULT(vkQueueWaitIdle(computeQueue))
			;
			break;
		case QUEUE_TYPE_TRANSFER:
			VK_CHECK_RESULT(vkQueueWaitIdle(transferQueue))
			;
			break;
		default:
			break;
	}
}

void VulkanRenderer::waitForDeviceIdle ()
{
	VK_CHECK_RESULT(vkDeviceWaitIdle(device));
}

bool VulkanRenderer::getFenceStatus (Fence fence)
{
	VkResult fenceStatus = vkGetFenceStatus(device, static_cast<VulkanFence*>(fence)->fenceHandle);

	switch (fenceStatus)
	{
		case VK_SUCCESS:
			return true;
		case VK_NOT_READY:
			return false;
		default:
			VK_CHECK_RESULT(fenceStatus)
			;
	}

	return false;
}

void VulkanRenderer::resetFence (Fence fence)
{
	VK_CHECK_RESULT(vkResetFences(device, 1, &static_cast<VulkanFence*>(fence)->fenceHandle));
}

void VulkanRenderer::resetFences (const std::vector<Fence> &fences)
{
	DEBUG_ASSERT(fences.size() > 0);

	std::vector<VkFence> vulkanFences(fences.size());

	for (size_t i = 0; i < fences.size(); i ++)
	{
		vulkanFences[i] = static_cast<VulkanFence*>(fences[i])->fenceHandle;
	}

	VK_CHECK_RESULT(vkResetFences(device, static_cast<uint32_t>(fences.size()), vulkanFences.data()));
}

void VulkanRenderer::waitForFence (Fence fence, double timeoutInSeconds)
{
	uint64_t timeoutInNanoseconds = static_cast<uint64_t>(timeoutInSeconds * 1.0e9);

	VkResult waitResult = vkWaitForFences(device, 1, &static_cast<VulkanFence*>(fence)->fenceHandle, VK_TRUE, timeoutInNanoseconds);

	switch (waitResult)
	{
		case VK_SUCCESS:
		case VK_TIMEOUT:
			return;
		default:
			VK_CHECK_RESULT(waitResult)
			;
	}
}

void VulkanRenderer::waitForFences (const std::vector<Fence> &fences, bool waitForAll, double timeoutInSeconds)
{
	DEBUG_ASSERT(fences.size() > 0);

	std::vector<VkFence> vulkanFences(fences.size());

	for (size_t i = 0; i < fences.size(); i ++)
	{
		vulkanFences[i] = static_cast<VulkanFence*>(fences[i])->fenceHandle;
	}

	uint64_t timeoutInNanoseconds = static_cast<uint64_t>(timeoutInSeconds * 1.0e9);

	VkResult waitResult = vkWaitForFences(device, static_cast<uint32_t>(fences.size()), vulkanFences.data(), waitForAll, timeoutInNanoseconds);

	switch (waitResult)
	{
		case VK_SUCCESS:
		case VK_TIMEOUT:
			return;
		default:
			VK_CHECK_RESULT(waitResult)
			;
	}
}

void VulkanRenderer::writeDescriptorSets (DescriptorSet dstSet, const std::vector<DescriptorWriteInfo> &writes)
{
	std::vector<VkDescriptorImageInfo> imageInfos; // To make sure pointers have the correct life-time
	std::vector<VkDescriptorBufferInfo> bufferInfos; // To make sure pointers have the correct life-time
	std::vector<VkWriteDescriptorSet> vkWrites;

	size_t im = 0, bf = 0;

	for (size_t i = 0; i < writes.size(); i ++)
	{
		const DescriptorWriteInfo &writeInfo = writes[i];

		switch (writeInfo.descriptorType)
		{
			case DESCRIPTOR_TYPE_CONSTANT_BUFFER:
			case DESCRIPTOR_TYPE_STORAGE_BUFFER:
				bf += writeInfo.bufferInfo.size();
				break;
			case DESCRIPTOR_TYPE_SAMPLER:
				im += writeInfo.samplerInfo.size();
				break;
			case DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			case DESCRIPTOR_TYPE_SAMPLED_TEXTURE:
			case DESCRIPTOR_TYPE_STORAGE_TEXTURE:
				im += writeInfo.textureInfo.size();
				break;
		}
	}

	imageInfos.reserve(im);
	bufferInfos.reserve(bf);

	for (size_t i = 0; i < writes.size(); i ++)
	{
		const DescriptorWriteInfo &writeInfo = writes[i];
		VkWriteDescriptorSet write = {};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.pNext = nullptr;
		write.dstSet = static_cast<VulkanDescriptorSet*>(dstSet)->setHandle;
		write.dstArrayElement = writeInfo.dstArrayElement;
		write.dstBinding = writeInfo.dstBinding;
		write.descriptorType = toVkDescriptorType(writeInfo.descriptorType);

		switch (writeInfo.descriptorType)
		{
			case DESCRIPTOR_TYPE_CONSTANT_BUFFER:
			{
				for (size_t a = 0; a < writeInfo.bufferInfo.size(); a++)
				{
					VkDescriptorBufferInfo bufferInfo = {};
					bufferInfo.buffer = static_cast<VulkanBuffer *>(writeInfo.bufferInfo[a].buffer)->bufferHandle;
					bufferInfo.offset = static_cast<VkDeviceSize>(writeInfo.bufferInfo[a].offset);
					bufferInfo.range = static_cast<VkDeviceSize>(writeInfo.bufferInfo[a].range);

					bufferInfos.push_back(bufferInfo);
				}

				write.pBufferInfo = &bufferInfos[bufferInfos.size() - writeInfo.bufferInfo.size()];
				write.descriptorCount = writeInfo.bufferInfo.size();

				break;
			}
			case DESCRIPTOR_TYPE_SAMPLER:
			{
				for (size_t a = 0; a < writeInfo.samplerInfo.size(); a++)
				{
					VkDescriptorImageInfo imageInfo = {};
					imageInfo.sampler = static_cast<VulkanSampler *>(writeInfo.samplerInfo[a].sampler)->samplerHandle;

					imageInfos.push_back(imageInfo);
				}

				write.pImageInfo = &imageInfos[imageInfos.size() - writeInfo.samplerInfo.size()];
				write.descriptorCount = writeInfo.samplerInfo.size();

				break;
			}
			case DESCRIPTOR_TYPE_SAMPLED_TEXTURE:
			{
				for (size_t a = 0; a < writeInfo.textureInfo.size(); a++)
				{
					VkDescriptorImageInfo imageInfo = {};
					imageInfo.imageView = static_cast<VulkanTextureView *>(writeInfo.textureInfo[a].view)->imageView;
					imageInfo.imageLayout = toVkImageLayout(writeInfo.textureInfo[a].layout);

					imageInfos.push_back(imageInfo);
				}

				write.pImageInfo = &imageInfos[imageInfos.size() - writeInfo.textureInfo.size()];
				write.descriptorCount = writeInfo.textureInfo.size();

				break;
			}
			case DESCRIPTOR_TYPE_STORAGE_BUFFER:
			{
				for (size_t a = 0; a < writeInfo.bufferInfo.size(); a++)
				{
					VkDescriptorBufferInfo bufferInfo = {};
					bufferInfo.buffer = static_cast<VulkanBuffer *>(writeInfo.bufferInfo[a].buffer)->bufferHandle;
					bufferInfo.offset = static_cast<VkDeviceSize>(writeInfo.bufferInfo[a].offset);
					bufferInfo.range = static_cast<VkDeviceSize>(writeInfo.bufferInfo[a].range);

					bufferInfos.push_back(bufferInfo);
				}

				write.pBufferInfo = &bufferInfos[bufferInfos.size() - writeInfo.bufferInfo.size()];
				write.descriptorCount = writeInfo.bufferInfo.size();

				break;
			}
			case DESCRIPTOR_TYPE_STORAGE_TEXTURE:
			{
				for (size_t a = 0; a < writeInfo.textureInfo.size(); a++)
				{
					VkDescriptorImageInfo imageInfo = {};
					imageInfo.imageView = static_cast<VulkanTextureView *>(writeInfo.textureInfo[a].view)->imageView;
					imageInfo.imageLayout = toVkImageLayout(writeInfo.textureInfo[a].layout);

					imageInfos.push_back(imageInfo);
				}

				write.pImageInfo = &imageInfos[imageInfos.size() - writeInfo.textureInfo.size()];
				write.descriptorCount = writeInfo.textureInfo.size();

				break;
			}
		}

		vkWrites.push_back(write);
	}

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(vkWrites.size()), vkWrites.data(), 0, nullptr);
}

RenderPass VulkanRenderer::createRenderPass (const std::vector<AttachmentDescription> &attachments, const std::vector<SubpassDescription> &subpasses, const std::vector<SubpassDependency> &dependencies)
{
	VulkanRenderPass *renderPass = new VulkanRenderPass();
	renderPass->attachments = attachments;
	renderPass->subpasses = subpasses;
	renderPass->subpassDependencies = dependencies;

	std::vector<VkAttachmentDescription> vkAttachments;
	std::vector<VkSubpassDescription> vkSubpasses;
	std::vector<VkSubpassDependency> vkDependencies;

	for (size_t i = 0; i < attachments.size(); i ++)
	{
		const AttachmentDescription &attachment = attachments[i];
		VkAttachmentDescription vkAttachment = {};
		vkAttachment.format = ResourceFormatToVkFormat(attachment.format);
		vkAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		vkAttachment.loadOp = toVkAttachmentLoadOp(attachment.loadOp);
		vkAttachment.storeOp = toVkAttachmentStoreOp(attachment.storeOp);
		vkAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		vkAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		vkAttachment.initialLayout = toVkImageLayout(attachment.initialLayout);
		vkAttachment.finalLayout = toVkImageLayout(attachment.finalLayout);

		vkAttachments.push_back(vkAttachment);
	}

	// These are needed so the pointers in the render pass create info point to data which is still in scope
	std::vector<std::vector<VkAttachmentReference> > subpass_vkColorAttachments;
	std::vector<std::vector<VkAttachmentReference> > subpass_vkInputAttachments;
	std::vector<VkAttachmentReference> subpass_vkDepthAttachment(subpasses.size());
	std::vector<std::vector<uint32_t> > subpass_vkPreserveAttachments;

	for (size_t i = 0; i < subpasses.size(); i ++)
	{
		const SubpassDescription &subpass = subpasses[i];
		VkSubpassDescription vkSubpass = {};
		vkSubpass.pipelineBindPoint = toVkPipelineBindPoint(subpass.bindPoint);
		vkSubpass.colorAttachmentCount = static_cast<uint32_t>(subpass.colorAttachments.size());
		vkSubpass.inputAttachmentCount = static_cast<uint32_t>(subpass.inputAttachments.size());
		vkSubpass.preserveAttachmentCount = static_cast<uint32_t>(subpass.preserveAttachments.size());
		vkSubpass.pDepthStencilAttachment = nullptr;

		subpass_vkColorAttachments.push_back(std::vector<VkAttachmentReference>());
		subpass_vkInputAttachments.push_back(std::vector<VkAttachmentReference>());
		subpass_vkPreserveAttachments.push_back(subpass.preserveAttachments);

		for (size_t j = 0; j < subpass.colorAttachments.size(); j ++)
		{
			const AttachmentReference &ref = subpass.colorAttachments[j];
			VkAttachmentReference vkRef = {};
			vkRef.attachment = ref.attachment;
			vkRef.layout = toVkImageLayout(ref.layout);

			subpass_vkColorAttachments[i].push_back(vkRef);
		}

		for (size_t j = 0; j < subpass.inputAttachments.size(); j ++)
		{
			const AttachmentReference &ref = subpass.inputAttachments[j];
			VkAttachmentReference vkRef = {};
			vkRef.attachment = ref.attachment;
			vkRef.layout = toVkImageLayout(ref.layout);

			subpass_vkInputAttachments[i].push_back(vkRef);
		}

		if (subpass.hasDepthAttachment)
		{
			VkAttachmentReference vkRef = {};
			vkRef.attachment = subpass.depthStencilAttachment.attachment;
			vkRef.layout = toVkImageLayout(subpass.depthStencilAttachment.layout);

			subpass_vkDepthAttachment[i] = vkRef;

			vkSubpass.pDepthStencilAttachment = &subpass_vkDepthAttachment[i];
		}

		vkSubpass.pColorAttachments = subpass_vkColorAttachments[i].data();
		vkSubpass.pInputAttachments = subpass_vkInputAttachments[i].data();
		vkSubpass.pPreserveAttachments = subpass_vkPreserveAttachments[i].data();

		vkSubpasses.push_back(vkSubpass);
	}

	for (size_t i = 0; i < dependencies.size(); i ++)
	{
		const SubpassDependency &dependency = dependencies[i];
		VkSubpassDependency vkDependency = {};
		vkDependency.srcSubpass = dependency.srcSubpass;
		vkDependency.dstSubpass = dependency.dstSubpass;
		vkDependency.srcAccessMask = toVkAccessFlags(dependency.srcAccessMask);
		vkDependency.dstAccessMask = toVkAccessFlags(dependency.dstAccessMask);
		vkDependency.srcStageMask = toVkPipelineStageFlags(dependency.srcStageMask);
		vkDependency.dstStageMask = toVkPipelineStageFlags(dependency.dstStageMask);
		vkDependency.dependencyFlags = dependency.byRegionDependency ? VK_DEPENDENCY_BY_REGION_BIT : 0;

		vkDependencies.push_back(vkDependency);
	}

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(vkAttachments.size());
	renderPassCreateInfo.pAttachments = vkAttachments.data();
	renderPassCreateInfo.subpassCount = static_cast<uint32_t>(vkSubpasses.size());
	renderPassCreateInfo.pSubpasses = vkSubpasses.data();
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(vkDependencies.size());
	renderPassCreateInfo.pDependencies = vkDependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassCreateInfo, nullptr, &renderPass->renderPassHandle));

	return renderPass;
}

RenderGraph VulkanRenderer::createRenderGraph()
{
	VulkanRenderGraph *vkRenderGraph = new VulkanRenderGraph(this);

	return vkRenderGraph;
}

Framebuffer VulkanRenderer::createFramebuffer (RenderPass renderPass, const std::vector<TextureView> &attachments, uint32_t width, uint32_t height, uint32_t layers)
{
	std::vector<VkImageView> imageAttachments = {};

	for (size_t i = 0; i < attachments.size(); i ++)
	{
		imageAttachments.push_back(static_cast<VulkanTextureView*>(attachments[i])->imageView);
	}

	VkFramebufferCreateInfo framebufferCreateInfo = {};
	framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferCreateInfo.renderPass = static_cast<VulkanRenderPass*>(renderPass)->renderPassHandle;
	framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(imageAttachments.size());
	framebufferCreateInfo.pAttachments = imageAttachments.data();
	framebufferCreateInfo.width = width;
	framebufferCreateInfo.height = height;
	framebufferCreateInfo.layers = layers;

	VulkanFramebuffer *framebuffer = new VulkanFramebuffer();

	VK_CHECK_RESULT(vkCreateFramebuffer(device, &framebufferCreateInfo, nullptr, &framebuffer->framebufferHandle));

	return framebuffer;
}

ShaderModule VulkanRenderer::createShaderModule (const std::string &file, ShaderStageFlagBits stage, ShaderSourceLanguage sourceLang, const std::string &entryPoint)
{
	VulkanShaderModule *vulkanShader = new VulkanShaderModule();

#ifdef __linux__
	vulkanShader->module = VulkanShaderLoader::createVkShaderModule(device, VulkanShaderLoader::compileGLSL(*defaultCompiler, file, toVkShaderStageFlagBits(stage)));
#elif defined(_WIN32)
	vulkanShader->module = VulkanShaderLoader::createVkShaderModule(device, VulkanShaderLoader::compileGLSL(file, toVkShaderStageFlagBits(stage), sourceLang, entryPoint));
#endif
	vulkanShader->stage = stage;
	vulkanShader->entryPoint = entryPoint;

	/*
	 * I'm formatting the debug textureName to trim it to the "GameData/shaders/" directory. For example:
	 * "GameData/shaders/vulkan/temp-swapchain.glsl" would turn to ".../vulkan/temp-swapchain.glsl"
	 */

	size_t i = file.find("/shaders/");

	std::string debugMarkerName = ".../";

	if (i != file.npos)
		debugMarkerName += file.substr(i + 9);

	debugMarkerSetName(device, vulkanShader->module, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, debugMarkerName);

	return vulkanShader;
}

ShaderModule VulkanRenderer::createShaderModuleFromSource (const std::string &source, const std::string &referenceName, ShaderStageFlagBits stage, ShaderSourceLanguage sourceLang, const std::string &entryPoint)
{
	VulkanShaderModule *vulkanShader = new VulkanShaderModule();

#ifdef __linux__
	vulkanShader->module = VulkanShaderLoader::createVkShaderModule(device, VulkanShaderLoader::compileGLSLFromSource(*defaultCompiler, source, referenceName, toVkShaderStageFlagBits(stage)));
#elif defined(_WIN32)
	vulkanShader->module = VulkanShaderLoader::createVkShaderModule(device, VulkanShaderLoader::compileGLSLFromSource(source, referenceName, toVkShaderStageFlagBits(stage), sourceLang, entryPoint));
#endif
	vulkanShader->stage = stage;

	/*
	 * I'm formatting the debug textureName to trim it to the "GameData/shaders/" directory. For example:
	 * "GameData/shaders/vulkan/temp-swapchain.glsl" would turn to ".../vulkan/temp-swapchain.glsl"
	 */

	size_t i = referenceName.find("/shaders/");

	std::string debugMarkerName = ".../";

	if (i != referenceName.npos)
		debugMarkerName += referenceName.substr(i + 9);

	debugMarkerSetName(device, vulkanShader->module, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, debugMarkerName);

	return vulkanShader;
}


Pipeline VulkanRenderer::createGraphicsPipeline (const GraphicsPipelineInfo &pipelineInfo, RenderPass renderPass, uint32_t subpass)
{
	return pipelineHandler->createGraphicsPipeline(pipelineInfo, renderPass, subpass);
}

Pipeline VulkanRenderer::createComputePipeline(const ComputePipelineInfo &pipelineInfo)
{
	return pipelineHandler->createComputePipeline(pipelineInfo);
}

DescriptorPool VulkanRenderer::createDescriptorPool (const DescriptorSetLayoutDescription &descriptorSetLayout, uint32_t poolBlockAllocSize)
{
	return new VulkanDescriptorPool(this, descriptorSetLayout, poolBlockAllocSize);
}

Fence VulkanRenderer::createFence (bool createAsSignaled)
{
	VulkanFence *vulkanFence = new VulkanFence();

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = createAsSignaled ? VK_FENCE_CREATE_SIGNALED_BIT : 0;

	VK_CHECK_RESULT(vkCreateFence(device, &fenceCreateInfo, nullptr, &vulkanFence->fenceHandle));

	return vulkanFence;
}

Semaphore VulkanRenderer::createSemaphore ()
{
	VulkanSemaphore *vulkanSem = new VulkanSemaphore();

	VkSemaphoreCreateInfo semCreateInfo = {};
	semCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VK_CHECK_RESULT(vkCreateSemaphore(device, &semCreateInfo, nullptr, &vulkanSem->semHandle));

	return vulkanSem;
}

std::vector<Semaphore> VulkanRenderer::createSemaphores (uint32_t count)
{
	std::vector<Semaphore> sems;

	for (uint32_t i = 0; i < count; i ++)
		sems.push_back(createSemaphore());

	return sems;
}

Texture VulkanRenderer::createTexture (suvec3 extent, ResourceFormat format, TextureUsageFlags usage, MemoryUsage memUsage, bool ownMemory, uint32_t mipLevelCount, uint32_t arrayLayerCount, uint32_t multiSampleCount)
{
#if VULKAN_DEBUG_COMPATIBILITY_CHECKS
	DEBUG_ASSERT(!(extent.z > 1 && arrayLayerCount > 1) && "3D Texture arrays are not supported, only one of extent.z or arrayLayerCount can be greater than 1");

	if ((format == RESOURCE_FORMAT_A2R10G10B10_UINT_PACK32 || format == RESOURCE_FORMAT_A2R10G10B10_UNORM_PACK32) && (usage & TEXTURE_USAGE_TRANSFER_DST_BIT))
		Log::get()->warn("There are no swizzle equivalents between D3D12 and Vulkan, using R10G10B10A2 and A2R10G10B10, respectively, and the renderer backend does not convert the texture data for you");

	if (format == RESOURCE_FORMAT_B10G11R11_UFLOAT_PACK32 && (usage & TEXTURE_USAGE_TRANSFER_DST_BIT))
		Log::get()->warn("There are no swizzle equivalents between D3D12 and Vulkan, using R11G11B10 and B10G11R11, respectively, and the renderer backend does not convert the texture data for you");

	if (format == RESOURCE_FORMAT_E5B9G9R9_UFLOAT_PACK32 && (usage & TEXTURE_USAGE_TRANSFER_DST_BIT))
		Log::get()->warn("There are no swizzle equivalents between D3D12 and Vulkan, using R9G9B9E5 and E5B9G9R9, respectively, and the renderer backend does not convert the texture data for you");

#endif

	VulkanTexture *tex = new VulkanTexture();
	tex->width = extent.x;
	tex->height = extent.y;
	tex->depth = extent.z;
	tex->textureFormat = format;
	tex->usage = TextureUsageFlagsToVkImageUsageFlags(usage);
	tex->layerCount = arrayLayerCount;
	tex->mipCount = mipLevelCount;

	VkImageCreateFlags createFlags = 0;

	if (arrayLayerCount == 6)
		createFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.extent = {extent.x, extent.y, extent.z};
	imageCreateInfo.imageType = extent.z > 1 ? VK_IMAGE_TYPE_3D : (extent.y > 1 ? VK_IMAGE_TYPE_2D : VK_IMAGE_TYPE_1D);
	imageCreateInfo.mipLevels = mipLevelCount;
	imageCreateInfo.arrayLayers = arrayLayerCount;
	imageCreateInfo.format = ResourceFormatToVkFormat(format);
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = TextureUsageFlagsToVkImageUsageFlags(usage);
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.flags = createFlags;
	imageCreateInfo.samples = sampleCountToVkSampleCount(multiSampleCount);

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = toVmaMemoryUsage(memUsage);
	allocInfo.flags = (ownMemory ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0);

	VK_CHECK_RESULT(vmaCreateImage(memAllocator, &imageCreateInfo, &allocInfo, &tex->imageHandle, &tex->imageMemory, nullptr));

	return tex;
}

TextureView VulkanRenderer::createTextureView (Texture texture, TextureViewType viewType, TextureSubresourceRange subresourceRange, ResourceFormat viewFormat)
{
	VulkanTexture *vkTex = static_cast<VulkanTexture*>(texture);

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = vkTex->imageHandle;
	imageViewCreateInfo.viewType = toVkImageViewType(viewType);
	imageViewCreateInfo.format = viewFormat == RESOURCE_FORMAT_UNDEFINED ? ResourceFormatToVkFormat(vkTex->textureFormat) : ResourceFormatToVkFormat(viewFormat);
	imageViewCreateInfo.subresourceRange.aspectMask = isVkDepthFormat(imageViewCreateInfo.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = subresourceRange.baseMipLevel;
	imageViewCreateInfo.subresourceRange.levelCount = subresourceRange.levelCount;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = subresourceRange.baseArrayLayer;
	imageViewCreateInfo.subresourceRange.layerCount = subresourceRange.layerCount;

	VulkanTextureView* vkTexView = new VulkanTextureView();
	vkTexView->parentTexture = texture;
	vkTexView->viewFormat = viewFormat == RESOURCE_FORMAT_UNDEFINED ? vkTex->textureFormat : viewFormat;
	vkTexView->viewType = viewType;
	vkTexView->baseMip = subresourceRange.baseMipLevel;
	vkTexView->mipCount = subresourceRange.levelCount;
	vkTexView->baseLayer = subresourceRange.baseArrayLayer;
	vkTexView->layerCount = subresourceRange.layerCount;

	VK_CHECK_RESULT(vkCreateImageView(device, &imageViewCreateInfo, nullptr, &vkTexView->imageView));

	return vkTexView;
}

Sampler VulkanRenderer::createSampler (SamplerAddressMode addressMode, SamplerFilter minFilter, SamplerFilter magFilter, float maxAnisotropy, svec3 min_max_biasLod, SamplerMipmapMode mipmapMode)
{
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.minFilter = toVkFilter(minFilter);
	samplerCreateInfo.magFilter = toVkFilter(magFilter);
	samplerCreateInfo.addressModeU = toVkSamplerAddressMode(addressMode);
	samplerCreateInfo.addressModeV = toVkSamplerAddressMode(addressMode);
	samplerCreateInfo.addressModeW = toVkSamplerAddressMode(addressMode);
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.mipmapMode = toVkSamplerMipmapMode(mipmapMode);
	samplerCreateInfo.mipLodBias = min_max_biasLod.z;
	samplerCreateInfo.minLod = min_max_biasLod.x;
	samplerCreateInfo.maxLod = min_max_biasLod.y;

	if (deviceFeatures.samplerAnisotropy && maxAnisotropy > 1.0f)
	{
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
		samplerCreateInfo.maxAnisotropy = std::min<float>(maxAnisotropy, deviceProps.limits.maxSamplerAnisotropy);
	}
	else
	{
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
		samplerCreateInfo.maxAnisotropy = 1.0f;
	}

	VulkanSampler* vkSampler = new VulkanSampler();

	VK_CHECK_RESULT(vkCreateSampler(device, &samplerCreateInfo, nullptr, &vkSampler->samplerHandle));

	return vkSampler;
}

Buffer VulkanRenderer::createBuffer(size_t size, BufferUsageFlags usage, BufferLayout initialLayout, MemoryUsage memUsage, bool ownMemory)
{
	VulkanBuffer *vkBuffer = new VulkanBuffer();
	vkBuffer->memorySize = static_cast<VkDeviceSize>(size);
	vkBuffer->usage = usage;

	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferInfo.usage = 0;

	if (usage & BUFFER_USAGE_TRANSFER_SRC_BIT)
		bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	if (usage & BUFFER_USAGE_TRANSFER_DST_BIT)
		bufferInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	if (usage & BUFFER_USAGE_CONSTANT_BUFFER_BIT)
		bufferInfo.usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	if (usage & BUFFER_USAGE_STORAGE_BUFFER_BIT)
		bufferInfo.usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	if (usage & BUFFER_USAGE_INDEX_BUFFER_BIT)
		bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
	if (usage & BUFFER_USAGE_VERTEX_BUFFER_BIT)
		bufferInfo.usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	if (usage & BUFFER_USAGE_INDIRECT_BUFFER_BIT)
		bufferInfo.usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

#if VULKAN_DEBUG_COMPATIBILITY_CHECKS
	switch (initialLayout)
	{
		case BUFFER_LAYOUT_VERTEX_BUFFER:
			if (!(usage & BUFFER_USAGE_VERTEX_BUFFER_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_VERTEX_BUFFER without its usage containing BUFFER_USAGE_VERTEX_BUFFER_BIT");
			break;
		case BUFFER_LAYOUT_INDEX_BUFFER:
			if (!(usage & BUFFER_USAGE_INDEX_BUFFER_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_INDEX_BUFFER without its usage containing BUFFER_INDEX_VERTEX_BUFFER_BIT");
			break;
		case BUFFER_LAYOUT_CONSTANT_BUFFER:
			if (!(usage & BUFFER_USAGE_CONSTANT_BUFFER_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_CONSTANT_BUFFER without its usage containing BUFFER_USAGE_CONSTANT_BUFFER_BIT");
			break;
		case BUFFER_LAYOUT_VERTEX_CONSTANT_BUFFER:
			if (!(usage & BUFFER_USAGE_VERTEX_BUFFER_BIT) || !(usage & BUFFER_USAGE_CONSTANT_BUFFER_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_VERTEX_CONSTANT_BUFFER without its usage containing BUFFER_USAGE_VERTEX_BUFFER_BIT and BUFFER_USAGE_CONSTANT_BUFFER_BIT");
			break;
		case BUFFER_LAYOUT_VERTEX_INDEX_BUFFER:
			if (!(usage & BUFFER_USAGE_VERTEX_BUFFER_BIT) || !(usage & BUFFER_USAGE_INDEX_BUFFER_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_VERTEX_INDEX_BUFFER without its usage containing BUFFER_USAGE_VERTEX_BUFFER_BIT and BUFFER_USAGE_INDEX_BUFFER_BIT");
			break;
		case BUFFER_LAYOUT_VERTEX_INDEX_CONSTANT_BUFFER:
			if (!(usage & BUFFER_USAGE_VERTEX_BUFFER_BIT) && !(usage & BUFFER_USAGE_INDEX_BUFFER_BIT) && !(usage & BUFFER_USAGE_CONSTANT_BUFFER_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_VERTEX_INDEX_CONSTANT_BUFFER without its usage containing BUFFER_USAGE_VERTEX_BUFFER_BIT and BUFFER_USAGE_INDEX_BUFFER_BIT and BUFFER_USAGE_CONSTANT_BUFFER_BIT");
			break;
		case BUFFER_LAYOUT_GENERAL:
			if (!(usage & BUFFER_USAGE_STORAGE_BUFFER_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_GENERAL without its usage containing BUFFER_USAGE_STORAGE_BUFFER_BIT");
			break;
		case BUFFER_LAYOUT_INDIRECT_BUFFER:
			if (!(usage & BUFFER_USAGE_INDIRECT_BUFFER_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_INDIRECT_BUFFER without its usage containing BUFFER_USAGE_INDIRECT_BUFFER_BIT");
			break;
		case BUFFER_LAYOUT_TRANSFER_SRC_OPTIMAL:
			if (!(usage & BUFFER_USAGE_TRANSFER_SRC_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_TRANSFER_SRC_OPTIMAL without its usage containing BUFFER_USAGE_TRANSFER_SRC_BIT");
			break;
		case BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL:
			if (!(usage & BUFFER_USAGE_TRANSFER_DST_BIT))
				Log::get()->error("VulkanRenderer: Buffer cannot be in layout BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL without its usage containing BUFFER_USAGE_TRANSFER_DST_BIT");
			break;
	}

#endif

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = toVmaMemoryUsage(memUsage);
	allocInfo.flags = (ownMemory ? VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT : 0);

	VK_CHECK_RESULT(vmaCreateBuffer(memAllocator, &bufferInfo, &allocInfo, &vkBuffer->bufferHandle, &vkBuffer->bufferMemory, nullptr));

	return vkBuffer;
}

void *VulkanRenderer::mapBuffer (Buffer buffer)
{
	VulkanBuffer *vkBuffer = static_cast<VulkanBuffer*>(buffer);

	void *mappedBufferMemory = nullptr;

	VK_CHECK_RESULT(vmaMapMemory(memAllocator, vkBuffer->bufferMemory, &mappedBufferMemory));

	return mappedBufferMemory;
}

void VulkanRenderer::unmapBuffer(Buffer buffer)
{
	VulkanBuffer *vkBuffer = static_cast<VulkanBuffer*>(buffer);

	vmaUnmapMemory(memAllocator, vkBuffer->bufferMemory);
}

StagingBuffer VulkanRenderer::createStagingBuffer (size_t dataSize)
{
	VulkanStagingBuffer *stagingBuffer = new VulkanStagingBuffer();
	stagingBuffer->bufferSize = dataSize;

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = static_cast<VkDeviceSize>(dataSize);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	allocInfo.flags = 0;

	VK_CHECK_RESULT(vmaCreateBuffer(memAllocator, &bufferCreateInfo, &allocInfo, &stagingBuffer->bufferHandle, &stagingBuffer->bufferMemory, nullptr));

	return stagingBuffer;
}

StagingBuffer VulkanRenderer::createAndFillStagingBuffer (size_t dataSize, const void *data)
{
	StagingBuffer stagingBuffer = createStagingBuffer(dataSize);

	fillStagingBuffer(stagingBuffer, dataSize, data);

	return stagingBuffer;
}

void VulkanRenderer::fillStagingBuffer (StagingBuffer stagingBuffer, size_t dataSize, const void *data)
{
	VulkanStagingBuffer* vkStagingBuffer = static_cast<VulkanStagingBuffer*>(stagingBuffer);

	if (vkStagingBuffer->bufferSize < dataSize)
	{
		Log::get()->error("Error while mapping a staging buffer. Given data size ({}) is larger than staging buffer memory size ({})", dataSize, vkStagingBuffer->bufferSize);

		throw std::runtime_error("vulkan error - staging buffer mapping");
	}

	void *mappedBufferMemory = nullptr;

	VK_CHECK_RESULT(vmaMapMemory(memAllocator, vkStagingBuffer->bufferMemory, &mappedBufferMemory));
	memcpy(mappedBufferMemory, data, dataSize);
	vmaUnmapMemory(memAllocator, vkStagingBuffer->bufferMemory);
}

void *VulkanRenderer::mapStagingBuffer(StagingBuffer stagingBuffer)
{
	VulkanStagingBuffer* vkStagingBuffer = static_cast<VulkanStagingBuffer*>(stagingBuffer);
	void *mappedBufferMemory = nullptr;

	VK_CHECK_RESULT(vmaMapMemory(memAllocator, vkStagingBuffer->bufferMemory, &mappedBufferMemory));

	return mappedBufferMemory;
}

void VulkanRenderer::unmapStagingBuffer(StagingBuffer stagingBuffer)
{
	VulkanStagingBuffer* vkStagingBuffer = static_cast<VulkanStagingBuffer*>(stagingBuffer);
	vmaUnmapMemory(memAllocator, vkStagingBuffer->bufferMemory);
}

StagingTexture VulkanRenderer::createStagingTexture(suvec3 extent, ResourceFormat format, uint32_t mipLevelCount, uint32_t arrayLayerCount)
{
	VulkanStagingTexture *stagingTexture = new VulkanStagingTexture();
	stagingTexture->textureFormat = format;
	stagingTexture->width = extent.x;
	stagingTexture->height = extent.y;
	stagingTexture->depth = extent.z;
	stagingTexture->mipLevels = mipLevelCount;
	stagingTexture->arrayLayers = arrayLayerCount;
	stagingTexture->bufferSize = 0;

	// This part ensures that the buffer regions containing the image data have the optimal offset and row pitch alignment for copying
	for (uint32_t layer = 0; layer < arrayLayerCount; layer++)
	{
		for (uint32_t level = 0; level < mipLevelCount; level++)
		{
			uint32_t mipWidth = std::max<uint32_t>(stagingTexture->width >> level, 1);
			uint32_t mipHeight = std::max<uint32_t>(stagingTexture->height >> level, 1);
			uint32_t mipDepth = std::max<uint32_t>(stagingTexture->depth >> level, 1);

			uint32_t rowSize = (uint32_t) mipWidth * getResourceFormatBytesPerElement(format);
			uint32_t rowPitch = rowSize;// ((rowSize + physicalDeviceLimits.optimalBufferCopyRowPitchAlignment - 1) / physicalDeviceLimits.optimalBufferCopyRowPitchAlignment) * physicalDeviceLimits.optimalBufferCopyRowPitchAlignment;

			stagingTexture->subresourceOffets.push_back(stagingTexture->bufferSize);
			stagingTexture->subresourceRowSize.push_back(rowSize);
			stagingTexture->subresourceRowPitches.push_back(rowPitch);
			
			stagingTexture->bufferSize += (size_t) (((rowPitch * mipHeight * mipDepth + bestBufferCopyOffsetAlignment - 1) / bestBufferCopyOffsetAlignment) * bestBufferCopyOffsetAlignment);
		}
	}

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = static_cast<VkDeviceSize>(stagingTexture->bufferSize);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 0;

	VmaAllocationCreateInfo allocInfo = {};
	allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	allocInfo.flags = 0;

	VK_CHECK_RESULT(vmaCreateBuffer(memAllocator, &bufferCreateInfo, &allocInfo, &stagingTexture->bufferHandle, &stagingTexture->bufferMemory, nullptr));

	return stagingTexture;
}

void VulkanRenderer::fillStagingTextureSubresource(StagingTexture stagingTexture, const void *textureData, uint32_t mipLevel, uint32_t arrayLayer)
{
	VulkanStagingTexture *vkStagingTexture = static_cast<VulkanStagingTexture*>(stagingTexture);
	
	uint32_t subresourceIndex = arrayLayer * vkStagingTexture->mipLevels + mipLevel;
	uint32_t subresourceSlicePitch = vkStagingTexture->subresourceRowPitches[subresourceIndex] * std::max<uint32_t>(vkStagingTexture->height >> mipLevel, 1);

	char *mappedBufferMemory = nullptr;
	VK_CHECK_RESULT(vmaMapMemory(memAllocator, vkStagingTexture->bufferMemory, reinterpret_cast<void**>(&mappedBufferMemory)));

	mappedBufferMemory += vkStagingTexture->subresourceOffets[subresourceIndex];

	for (uint32_t z = 0; z < std::max<uint32_t>(vkStagingTexture->depth >> mipLevel, 1); z++)
	{
		char *dstSlice = mappedBufferMemory + subresourceSlicePitch * z;
		const char *srcSlice = reinterpret_cast<const char*>(textureData) + vkStagingTexture->subresourceRowSize[subresourceIndex] * std::max<uint32_t>(vkStagingTexture->height >> mipLevel, 1) * z;

		for (uint32_t y = 0; y < std::max<uint32_t>(vkStagingTexture->width >> mipLevel, 1); y++)
		{
			memcpy(dstSlice + vkStagingTexture->subresourceRowPitches[subresourceIndex] * y, srcSlice + vkStagingTexture->subresourceRowSize[subresourceIndex] * y, vkStagingTexture->subresourceRowSize[subresourceIndex]);
		}
	}

	vmaUnmapMemory(memAllocator, vkStagingTexture->bufferMemory);
}

void VulkanRenderer::destroyCommandPool (CommandPool pool)
{
	VulkanCommandPool *vkCmdPool = static_cast<VulkanCommandPool*>(pool);

	if (vkCmdPool->poolHandle != VK_NULL_HANDLE)
		vkDestroyCommandPool(device, vkCmdPool->poolHandle, nullptr);

	delete pool;
}

void VulkanRenderer::destroyRenderGraph(RenderGraph &graph)
{
	delete graph;
	graph = nullptr;
}

void VulkanRenderer::destroyPipeline (Pipeline pipeline)
{
	VulkanPipeline *vulkanPipeline = static_cast<VulkanPipeline*>(pipeline);

	if (vulkanPipeline->pipelineHandle != VK_NULL_HANDLE)
	{
		vkDestroyPipeline(device, vulkanPipeline->pipelineHandle, nullptr);
		vkDestroyPipelineLayout(device, vulkanPipeline->pipelineLayoutHandle, nullptr);
	}

	delete pipeline;
}

void VulkanRenderer::destroyShaderModule (ShaderModule module)
{
	VulkanShaderModule *vulkanShader = static_cast<VulkanShaderModule*>(module);

	if (vulkanShader->module != VK_NULL_HANDLE)
		vkDestroyShaderModule(device, vulkanShader->module, nullptr);

	delete module;
}

void VulkanRenderer::destroyDescriptorPool (DescriptorPool pool)
{
	delete pool;
}

void VulkanRenderer::destroyTexture (Texture texture)
{
	VulkanTexture *vkTex = static_cast<VulkanTexture*>(texture);

	if (vkTex->imageHandle != VK_NULL_HANDLE)
		vmaDestroyImage(memAllocator, vkTex->imageHandle, vkTex->imageMemory);

	delete vkTex;
}

void VulkanRenderer::destroyTextureView (TextureView textureView)
{
	VulkanTextureView *vkTexView = static_cast<VulkanTextureView*>(textureView);

	if (vkTexView->imageView != VK_NULL_HANDLE)
		vkDestroyImageView(device, vkTexView->imageView, nullptr);

	delete vkTexView;
}

void VulkanRenderer::destroySampler (Sampler sampler)
{
	VulkanSampler *vkSampler = static_cast<VulkanSampler*>(sampler);

	if (vkSampler->samplerHandle != VK_NULL_HANDLE)
		vkDestroySampler(device, vkSampler->samplerHandle, nullptr);

	delete vkSampler;
}

void VulkanRenderer::destroyBuffer (Buffer buffer)
{
	VulkanBuffer *vkBuffer = static_cast<VulkanBuffer*>(buffer);

	if (vkBuffer->bufferHandle != VK_NULL_HANDLE)
		vmaDestroyBuffer(memAllocator, vkBuffer->bufferHandle, vkBuffer->bufferMemory);

	delete vkBuffer;
}

void VulkanRenderer::destroyStagingBuffer (StagingBuffer stagingBuffer)
{
	VulkanStagingBuffer *vkBuffer = static_cast<VulkanStagingBuffer*>(stagingBuffer);

	if (vkBuffer->bufferHandle != VK_NULL_HANDLE)
		vmaDestroyBuffer(memAllocator, vkBuffer->bufferHandle, vkBuffer->bufferMemory);

	delete vkBuffer;
}

void VulkanRenderer::destroyFence (Fence fence)
{
	VulkanFence *vulkanFence = static_cast<VulkanFence*>(fence);

	if (vulkanFence->fenceHandle != VK_NULL_HANDLE)
		vkDestroyFence(device, vulkanFence->fenceHandle, nullptr);

	delete vulkanFence;
}

void VulkanRenderer::destroySemaphore (Semaphore sem)
{
	VulkanSemaphore *vulkanSem = static_cast<VulkanSemaphore*>(sem);

	if (vulkanSem->semHandle != VK_NULL_HANDLE)
		vkDestroySemaphore(device, vulkanSem->semHandle, nullptr);

	delete vulkanSem;
}

void VulkanRenderer::destroyStagingTexture(StagingTexture stagingTexture)
{
	VulkanStagingTexture *vkStagingTexture = static_cast<VulkanStagingTexture*>(stagingTexture);

	if (vkStagingTexture->bufferHandle != VK_NULL_HANDLE)
		vmaDestroyBuffer(memAllocator, vkStagingTexture->bufferHandle, vkStagingTexture->bufferMemory);

	delete vkStagingTexture;
}

void VulkanRenderer::setObjectDebugName (void *obj, RendererObjectType objType, const std::string &name)
{
#if RENDER_DEBUG_MARKERS

	if (VulkanExtensions::enabled_VK_EXT_debug_marker)
	{
		uint64_t objHandle = 0;

		switch (objType)
		{
			case OBJECT_TYPE_SEMAPHORE:
				objHandle = (uint64_t) static_cast<VulkanSemaphore*>(obj)->semHandle;
			case OBJECT_TYPE_COMMAND_BUFFER:
				objHandle = (uint64_t) static_cast<VulkanCommandBuffer*>(obj)->bufferHandle;
				break;
			case OBJECT_TYPE_FENCE:
				objHandle = (uint64_t) static_cast<VulkanFence*>(obj)->fenceHandle;
				break;
			case OBJECT_TYPE_BUFFER:
				objHandle = (uint64_t) static_cast<VulkanBuffer*>(obj)->bufferHandle;
				break;
			case OBJECT_TYPE_TEXTURE:
				objHandle = (uint64_t) static_cast<VulkanTexture*>(obj)->imageHandle;
				break;
			case OBJECT_TYPE_TEXTURE_VIEW:
				objHandle = (uint64_t) static_cast<VulkanTextureView*>(obj)->imageView;
				break;
			case OBJECT_TYPE_SHADER_MODULE:
				objHandle = (uint64_t) static_cast<VulkanShaderModule*>(obj)->module;
				break;
			case OBJECT_TYPE_PIPELINE:
				objHandle = (uint64_t) static_cast<VulkanPipeline*>(obj)->pipelineHandle;
				break;
			case OBJECT_TYPE_SAMPLER:
				objHandle = (uint64_t) static_cast<VulkanSampler*>(obj)->samplerHandle;
				break;
			case OBJECT_TYPE_DESCRIPTOR_SET:
				objHandle = (uint64_t) static_cast<VulkanDescriptorSet*>(obj)->setHandle;
				break;
			case OBJECT_TYPE_COMMAND_POOL:
				objHandle = (uint64_t) static_cast<VulkanCommandPool*>(obj)->poolHandle;
				break;
			
		}

		VkDebugMarkerObjectNameInfoEXT nameInfo = {};
		nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
		nameInfo.objectType = toVkDebugReportObjectTypeEXT(objType);
		nameInfo.object = objHandle;
		nameInfo.pObjectName = name.c_str();

		vkDebugMarkerSetObjectNameEXT(device, &nameInfo);
	}

#endif
}

void VulkanRenderer::initSwapchain (Window *wnd)
{
	swapchains->initSwapchain(wnd);
}

void VulkanRenderer::presentToSwapchain (Window *wnd, std::vector<Semaphore> waitSemaphores)
{
	std::vector<VkSemaphore> vulkanWaitSems;

	for (Semaphore sem : waitSemaphores)
		vulkanWaitSems.push_back(static_cast<VulkanSemaphore*>(sem)->semHandle);

	swapchains->presentToSwapchain(wnd, vulkanWaitSems);
}

void VulkanRenderer::recreateSwapchain (Window *wnd)
{
	swapchains->recreateSwapchain(wnd);
}

void VulkanRenderer::setSwapchainTexture (Window *wnd, TextureView texView, Sampler sampler, TextureLayout layout)
{
	swapchains->setSwapchainSourceImage(wnd, static_cast<VulkanTextureView*>(texView)->imageView, static_cast<VulkanSampler*>(sampler)->samplerHandle, toVkImageLayout(layout));
}

bool VulkanRenderer::areValidationLayersEnabled ()
{
	return validationLayersEnabled;
}

void VulkanRenderer::createLogicalDevice ()
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

	float queuePriority = 1.0f;
	for (size_t i = 0; i < deviceQueueInfo.uniqueQueueFamilies.size(); i ++)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = deviceQueueInfo.uniqueQueueFamilies[i];
		queueCreateInfo.queueCount = deviceQueueInfo.uniqueQueueFamilyQueueCount[i];
		queueCreateInfo.pQueuePriorities = &queuePriority;
		queueCreateInfos.push_back(queueCreateInfo);
	}

	std::vector<const char*> enabledDeviceExtensions;
	enabledDeviceExtensions.insert(enabledDeviceExtensions.begin(), deviceExtensions.begin(), deviceExtensions.end());

	for (const char *ext : enabledDeviceExtensions)
		Log::get()->info("Enabling the {} extension", std::string(ext));

	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

	// Check for debug marker support (aka, we're running under RenderDoc)
	for (auto ext : availableExtensions)
	{
		if (!VulkanExtensions::enabled_VK_EXT_debug_marker && RENDER_DEBUG_MARKERS && strcmp(ext.extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME) == 0)
		{
			enabledDeviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
			VulkanExtensions::enabled_VK_EXT_debug_marker = true;
			Log::get()->info("Enabling the VK_EXT_debug_market extension");
		}
		else if (!VulkanExtensions::enabled_VK_AMD_rasterization_order && strcmp(ext.extensionName, VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME) == 0)
		{
			enabledDeviceExtensions.push_back(VK_AMD_RASTERIZATION_ORDER_EXTENSION_NAME);
			VulkanExtensions::enabled_VK_AMD_rasterization_order = true;
			Log::get()->info("Enabling the VK_AMD_rasterization_order extension");
		}
	}

	VkPhysicalDeviceFeatures enabledDeviceFeatures = {};
	enabledDeviceFeatures.samplerAnisotropy = deviceFeatures.samplerAnisotropy;
	enabledDeviceFeatures.logicOp = true;
	enabledDeviceFeatures.geometryShader = true;
	enabledDeviceFeatures.tessellationShader = true;
	enabledDeviceFeatures.fillModeNonSolid = true;
	enabledDeviceFeatures.textureCompressionBC = true;
	enabledDeviceFeatures.shaderImageGatherExtended = true;

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(enabledDeviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = enabledDeviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &enabledDeviceFeatures;

	if (validationLayersEnabled)
	{
		deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		deviceCreateInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		deviceCreateInfo.enabledLayerCount = 0;
	}

	VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device));

	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(physicalDevice, &props);

	physicalDeviceLimits = props.limits;

	bestBufferCopyOffsetAlignment = lcm<uint32_t>(4, physicalDeviceLimits.optimalBufferCopyOffsetAlignment);

	VulkanExtensions::getProcAddresses(device);

	vkGetDeviceQueue(device, deviceQueueInfo.graphicsFamily, deviceQueueInfo.graphicsQueue, &graphicsQueue);
	vkGetDeviceQueue(device, deviceQueueInfo.computeFamily, deviceQueueInfo.computeQueue, &computeQueue);
	vkGetDeviceQueue(device, deviceQueueInfo.transferFamily, deviceQueueInfo.transferQueue, &transferQueue);
	vkGetDeviceQueue(device, deviceQueueInfo.presentFamily, deviceQueueInfo.presentQueue, &presentQueue);
}

void VulkanRenderer::choosePhysicalDevice ()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0)
	{
		Log::get()->error("No devices were found that support Vulkan (deviceCount == 0)");
			
		throw std::runtime_error("vulkan error - no devices found");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

	std::multimap<int, std::pair<VkPhysicalDevice, DeviceQueues> > validDevices; // A list of valid devices and their score to decide choosing on

	Log::get()->info("List of devices and their queue families");

	for (const auto& physDevice : devices)
	{
		// Print out the device & it's basic properties for debugging purposes
		VkPhysicalDeviceProperties props = {};
		vkGetPhysicalDeviceProperties(physDevice, &props);
		
		Log::get()->info("\tDevice: {}, Vendor: {}, Type: {}", props.deviceName, getVkVendorString(props.vendorID).c_str(), props.deviceType);

		DeviceQueues queues = findQueueFamilies(physDevice);
		bool extensionsSupported = checkDeviceExtensionSupport(physDevice, deviceExtensions);

		if (!queues.isComplete() || !extensionsSupported)
			continue;

		SwapchainSupportDetails swapChainDetails = swapchains->querySwapchainSupport(physDevice, swapchains->swapchains[onAllocInfo.mainWindow].surface);

		if (swapChainDetails.formats.empty() || swapChainDetails.presentModes.empty())
			continue;

		VkPhysicalDeviceFeatures feats = {};
		vkGetPhysicalDeviceFeatures(physDevice, &feats);

		// These are features necessary for this engine
		if (!feats.tessellationShader || !feats.textureCompressionBC)
		{
			continue;
		}

		/*
		 * So far the device we're iterating over has all the necessary features, so now we'll
		 * try and pick the best based on a score.
		 */

		uint32_t score = 0;
		
		score += props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 500 : 0;
		score += props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 200 : 0;
		score += props.limits.maxSamplerAnisotropy;
	
		validDevices.insert(std::make_pair(score, std::make_pair(physDevice, queues)));
	}

	if (validDevices.size() == 0)
	{
		Log::get()->error("Failed to find a valid device");

		throw std::runtime_error("vulkan error - failed to find a valid device");
	}
	else
	{
		physicalDevice = validDevices.rbegin()->second.first;
		deviceQueueInfo = validDevices.rbegin()->second.second;

		deviceQueueInfo.createUniqueQueueFamilyList();

		vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
		vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

		Log::get()->info("Chose device: {}, queue info:", deviceProps.deviceName);
		Log::get()->info("\tGFX/General: {}/{}, Compute: {}/{}, Transfer: {}/{}, Present: {}/{}", deviceQueueInfo.graphicsFamily, deviceQueueInfo.graphicsQueue, deviceQueueInfo.computeFamily, deviceQueueInfo.computeQueue, deviceQueueInfo.transferFamily, deviceQueueInfo.transferQueue,
						 deviceQueueInfo.presentFamily, deviceQueueInfo.presentQueue);
	}
}

bool VulkanRenderer::checkExtraSurfacePresentSupport(VkSurfaceKHR surface)
{
	VkBool32 presentSupport = false;
	vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, deviceQueueInfo.presentFamily, surface, &presentSupport);

	return presentSupport;
}

/*
 * Tries the find the optimal queue families & queues for a device and it's vendor. In general
 * it's best not to use more than one queue for a certain task, aka don't use 5 graphics queues
 * and 3 compute queues, because a game engine doesn't really benefit. Also, the main vendors (AMD, Nvidia, Intel)
 * tend to use the same queue family layout for each card, and the architectures for each card from each
 * vendor are usually similar. So, I just stick with some general rules for each vendor. Note that any
 * vendor besides Nvidia, AMD, or Intel I'm just gonna default to Intel's config.
 *
 *
 * On AMD 		- 1 general/graphics queue, 1 compute queue, 1 transfer queue
 * On Nvidia 	- 1 general/graphics/compute queue, 1 transfer queue
 * On Intel		- 1 queue for everything
 */
DeviceQueues VulkanRenderer::findQueueFamilies (VkPhysicalDevice physDevice)
{
	DeviceQueues families;

	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(physDevice, &props);

	// Intel iGPUs don't really benefit AT ALL from more than one queue, because shared memory and stuff
	bool uniqueCompute = false, uniqueTransfer = false;

	if (props.vendorID == VENDOR_ID_AMD)
	{
		// Leverage async compute & the dedicated transfer (which should map to the DMA)
		uniqueCompute = uniqueTransfer = true;
	}
	else if (props.vendorID == VENDOR_ID_NVIDIA)
	{
		// Nvidia doesn't really benefit much from async compute, but it does from a dedicated transfer (which also should map to the DMA)
		uniqueTransfer = true;
	}

	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physDevice, &queueFamilyCount, queueFamilies.data());

	// Print out the environment for debugging purposes
	for (int i = 0; i < (int) queueFamilyCount; i ++)
	{
		const auto& family = queueFamilies[i];

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, i, swapchains->swapchains[onAllocInfo.mainWindow].surface, &presentSupport);

		Log::get()->info("\tQueue family {}, w/ {}, queues. Graphics: {}, Compute: {}, Transfer: {}, Present: {}", i, family.queueCount, family.queueFlags & VK_QUEUE_GRAPHICS_BIT ? 1 : 0, family.queueFlags & VK_QUEUE_COMPUTE_BIT ? 1 : 0, family.queueFlags & VK_QUEUE_TRANSFER_BIT ? 1 : 0,
				  presentSupport);
	}

	// Try to find the general/graphics & present queue
	for (int i = 0; i < (int) queueFamilyCount; i ++)
	{
		const auto& family = queueFamilies[i];

		if (family.queueCount == 0)
			continue;

		if (families.graphicsFamily == -1 && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
		{
			families.graphicsFamily = i;
			families.graphicsQueue = 0;
		}

		VkBool32 presentSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(physDevice, i, swapchains->swapchains[onAllocInfo.mainWindow].surface, &presentSupport);

		if (families.presentFamily == -1)
		{
			families.presentFamily = i;
			families.presentQueue = 0;
		}
	}

	if (!uniqueCompute)
	{
		families.computeFamily = families.graphicsFamily;
		families.computeQueue = families.graphicsQueue;
	}
	else
	{
		// First we'll try to find a dedicated compute family
		for (int i = 0; i < (int) queueFamilyCount; i ++)
		{
			const auto& family = queueFamilies[i];

			if (family.queueCount == 0)
				continue;

			if ((family.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				families.computeFamily = i;
				families.computeQueue = 0;

				break;
			}
		}

		// If we didn't find a dedicated compute family, then just try and find a compute queue from a family that's unique
		// from the graphics queue
		if (families.computeFamily == -1)
		{
			for (int i = 0; i < (int) queueFamilyCount; i ++)
			{
				const auto& family = queueFamilies[i];

				if (family.queueCount == 0 || !(family.queueFlags & VK_QUEUE_COMPUTE_BIT) || families.computeFamily != -1)
					continue;

				for (uint32_t t = 0; t < family.queueCount; t ++)
				{
					if (!(i == families.graphicsFamily && t == families.graphicsQueue))
					{
						families.computeFamily = i;
						families.computeQueue = t;
						break;
					}
				}
			}
		}

		// If we still can't find a dedicated compute queue, then just default to the graphics/general queue
		if (families.computeFamily == -1)
		{
			families.computeFamily = families.graphicsFamily;
			families.computeQueue = families.graphicsQueue;
		}
	}

	if (!uniqueTransfer)
	{
		families.transferFamily = families.graphicsFamily;
		families.transferQueue = families.graphicsQueue;
	}
	else
	{
		// First we'll try to find a dedicated transfer family
		for (int i = 0; i < (int) queueFamilyCount; i ++)
		{
			const auto& family = queueFamilies[i];

			if (family.queueCount == 0)
				continue;

			if ((family.queueFlags & VK_QUEUE_TRANSFER_BIT) && !(family.queueFlags & VK_QUEUE_COMPUTE_BIT) && !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT))
			{
				families.transferFamily = i;
				families.transferQueue = 0;

				break;
			}
		}

		// If we didn't find a dedicated transfer family, then just try and find a transfer queue from a family that's unique
		// from the graphics & compute queues
		if (families.transferFamily == -1)
		{
			for (int i = 0; i < (int) queueFamilyCount; i ++)
			{
				const auto& family = queueFamilies[i];

				if (family.queueCount == 0 || !(family.queueFlags & VK_QUEUE_TRANSFER_BIT) || families.transferFamily != -1)
					continue;

				for (uint32_t t = 0; t < family.queueCount; t ++)
				{
					if (!((i == families.graphicsFamily && t == families.graphicsQueue) || (i == families.computeFamily && t == families.computeQueue)))
					{
						families.transferFamily = i;
						families.transferQueue = t;
						break;
					}
				}
			}
		}

		// If we still can't find a dedicated transfer queue, then just default to the graphics/general queue
		if (families.transferFamily == -1)
		{
			families.transferFamily = families.graphicsFamily;
			families.transferQueue = families.graphicsQueue;
		}
	}

	return families;
}

std::vector<const char*> VulkanRenderer::getInstanceExtensions ()
{
	std::vector<const char*> extensions;

	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (unsigned int i = 0; i < glfwExtensionCount; i ++)
	{
		extensions.push_back(glfwExtensions[i]);
	}

	if (validationLayersEnabled)
	{
		extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	return extensions;
}

bool VulkanRenderer::checkDeviceExtensionSupport (VkPhysicalDevice physDevice, std::vector<const char*> extensions)
{
	uint32_t extensionCount;
	vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extensionCount, nullptr);

	std::vector<VkExtensionProperties> availableExtensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(physDevice, nullptr, &extensionCount, availableExtensions.data());

	std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

	for (const auto& extension : availableExtensions)
	{
		requiredExtensions.erase(extension.extensionName);
	}

	return requiredExtensions.empty();
}

bool VulkanRenderer::checkValidationLayerSupport (std::vector<const char*> layers)
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : layers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}

VkBool32 VulkanRenderer::debugCallback (VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData)
{
	printf("[StarlightEngine] [VkDbg] ");

	if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT)
		printf("[Info] ");

	if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
		printf("[Warning] ");

	if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
		printf("[Perf] ");

	if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
		printf("[Error] ");

	if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT)
		printf("[Dbg] ");

	printf("Layer: %s gave message: %s\n", layerPrefix, msg);

	return VK_FALSE;
}

#endif
