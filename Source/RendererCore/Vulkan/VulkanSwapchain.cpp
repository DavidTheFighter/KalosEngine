#include "RendererCore/Vulkan/VulkanSwapchain.h"
#if BUILD_VULKAN_BACKEND

#include <Peripherals/Window.h>

#include <RendererCore/Vulkan/VulkanRenderer.h>
#include <RendererCore/Vulkan/VulkanShaderLoader.h>

#include <GLFW/glfw3.h>

VulkanSwapchain::VulkanSwapchain (VulkanRenderer* vulkanRendererParent)
{
	renderer = vulkanRendererParent;

	swapchainCommandPool = VK_NULL_HANDLE;
	swapchainVertShader = VK_NULL_HANDLE;
	swapchainFragShader = VK_NULL_HANDLE;
	swapchainPipelineLayout = VK_NULL_HANDLE;

	swapchainDescriptorLayout = VK_NULL_HANDLE;

	swapchainDummySourceImage = VK_NULL_HANDLE;
	swapchainDummySourceImageView = VK_NULL_HANDLE;
	swapchainDummySourceImageMemory = NULL;
	swapchainDummySampler = VK_NULL_HANDLE;

	mainWindow = vulkanRendererParent->onAllocInfo.mainWindow;

	WindowSwapchain swapchain = {};
	swapchain.window = mainWindow;
	VK_CHECK_RESULT(glfwCreateWindowSurface(renderer->instance, static_cast<GLFWwindow*>(mainWindow->getWindowObjectPtr()), nullptr, &swapchain.surface));

	swapchains[mainWindow] = swapchain;
}

VulkanSwapchain::~VulkanSwapchain ()
{
	destroySwapchain(mainWindow);
	vkDestroyDescriptorPool(renderer->device, swapchains[mainWindow].swapchainDescriptorPool, nullptr);
	vkDestroySemaphore(renderer->device, swapchains[mainWindow].swapchainNextImageAvailableSemaphore, nullptr);
	vkDestroySemaphore(renderer->device, swapchains[mainWindow].swapchainDoneRenderingSemaphore, nullptr);

	for (auto extraWindow : extraWindows)
	{
		destroySwapchain(extraWindow);

		vkDestroyDescriptorPool(renderer->device, swapchains[extraWindow].swapchainDescriptorPool, nullptr);
		vkDestroySemaphore(renderer->device, swapchains[extraWindow].swapchainNextImageAvailableSemaphore, nullptr);
		vkDestroySemaphore(renderer->device, swapchains[extraWindow].swapchainDoneRenderingSemaphore, nullptr);
	}

	vkDestroyCommandPool(renderer->device, swapchainCommandPool, nullptr);

	vmaDestroyImage(renderer->memAllocator, swapchainDummySourceImage, swapchainDummySourceImageMemory);
	vkDestroyImageView(renderer->device, swapchainDummySourceImageView, nullptr);
	vkDestroySampler(renderer->device, swapchainDummySampler, nullptr);

	vkDestroyDescriptorSetLayout(renderer->device, swapchainDescriptorLayout, nullptr);
	vkDestroyPipelineLayout(renderer->device, swapchainPipelineLayout, nullptr);

	vkDestroyShaderModule(renderer->device, swapchainVertShader, nullptr);
	vkDestroyShaderModule(renderer->device, swapchainFragShader, nullptr);
}

void VulkanSwapchain::init ()
{
	const std::string tempSwapchainShaderFile = "GameData/shaders/vulkan/temp-swapchain.glsl";

#ifdef __linux__
	swapchainVertShader = VulkanShaderLoader::createVkShaderModule(renderer->device, VulkanShaderLoader::compileGLSL(*renderer->defaultCompiler, renderer->workingDir + tempSwapchainShaderFile, VK_SHADER_STAGE_VERTEX_BIT));
	swapchainFragShader = VulkanShaderLoader::createVkShaderModule(renderer->device, VulkanShaderLoader::compileGLSL(*renderer->defaultCompiler, renderer->workingDir + tempSwapchainShaderFile, VK_SHADER_STAGE_FRAGMENT_BIT));
#elif defined(_WIN32)
	swapchainVertShader = VulkanShaderLoader::createVkShaderModule(renderer->device, VulkanShaderLoader::compileGLSL(tempSwapchainShaderFile, VK_SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_GLSL, "main"));
	swapchainFragShader = VulkanShaderLoader::createVkShaderModule(renderer->device, VulkanShaderLoader::compileGLSL(tempSwapchainShaderFile, VK_SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_GLSL, "main"));
#endif

	//swapchainVertShader = VulkanShaderLoader::createVkShaderModule(renderer->device, VulkanShaderLoader::loadSpv("GameData/shaders/vulkan/temp-swapchain.vert.spv"));
	//swapchainFragShader = VulkanShaderLoader::createVkShaderModule(renderer->device, VulkanShaderLoader::loadSpv("GameData/shaders/vulkan/temp-swapchain.frag.spv"));

	debugMarkerSetName(renderer->device, swapchainVertShader, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, ".../vulkan/temp-swapchain.glsl");
	debugMarkerSetName(renderer->device, swapchainFragShader, VK_DEBUG_REPORT_OBJECT_TYPE_SHADER_MODULE_EXT, ".../vulkan/temp-swapchain.glsl");

	VkCommandPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCreateInfo.queueFamilyIndex = renderer->deviceQueueInfo.presentFamily;

	VK_CHECK_RESULT(vkCreateCommandPool(renderer->device, &poolCreateInfo, nullptr, &swapchainCommandPool));

	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	samplerLayoutBinding.pImmutableSamplers = nullptr;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding textureLayoutBinding = {};
	textureLayoutBinding.binding = 1;
	textureLayoutBinding.descriptorCount = 1;
	textureLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	textureLayoutBinding.pImmutableSamplers = nullptr;
	textureLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	std::vector<VkDescriptorSetLayoutBinding> bindings = {samplerLayoutBinding, textureLayoutBinding};

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
	layoutInfo.pBindings = bindings.data();

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(renderer->device, &layoutInfo, nullptr, &swapchainDescriptorLayout));

	std::vector<VkDescriptorSetLayout> setLayouts = {swapchainDescriptorLayout};

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	VK_CHECK_RESULT(vkCreatePipelineLayout(renderer->device, &pipelineLayoutInfo, nullptr, &swapchainPipelineLayout));
	debugMarkerSetName(renderer->device, swapchainPipelineLayout, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_LAYOUT_EXT, "Swapchain Graphics Pipeline Layout");

	// Initialize the dummy images so that it doesn't throw an error when we build the command buffers before the user has specified an input image
	createDummySourceImage();

	// Initialize the swapchain for the main window
	//initSwapchain(mainWindow);
}

void VulkanSwapchain::initSwapchain (Window *wnd)
{
	WindowSwapchain swapchain = {};
	swapchain.window = wnd;

	auto it = swapchains.find(wnd);

	if (it == swapchains.end())
	{
		// Only extra windows should be here, as the main window is partially pre-inited in the constructor

		VK_CHECK_RESULT(glfwCreateWindowSurface(renderer->instance, static_cast<GLFWwindow*>(wnd->getWindowObjectPtr()), nullptr, &swapchain.surface));

		// If the new surface we created isn't compatible with the current physical device & queues, then we won't bother trying to use it
		if (!renderer->checkExtraSurfacePresentSupport(swapchain.surface))
		{
			vkDestroySurfaceKHR(renderer->instance, swapchain.surface, nullptr);

			Log::get()->error("Couldn't create a compatible swapchain for extra window: {} (Title: {})", (void*) wnd, wnd->getTitle());

			return;
		}
	}
	else
	{
		swapchain = it->second;
	}

	createDescriptors(swapchain);
	createSyncPrimitives(swapchain);

	swapchains[wnd] = swapchain;

	if (wnd != mainWindow)
	{
		extraWindows.push_back(wnd);
	}

	setSwapchainSourceImage(wnd, swapchainDummySourceImageView, swapchainDummySampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, false);
	createSwapchain(wnd);
}

void VulkanSwapchain::presentToSwapchain (Window *wnd, std::vector<VkSemaphore> externalWaitSemaphores)
{
	vkQueueWaitIdle(renderer->presentQueue);

	WindowSwapchain &swapchain = swapchains[wnd];

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(renderer->device, swapchain.swapchain, std::numeric_limits<uint64_t>::max(), swapchain.swapchainNextImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		// TODO Implement out of date swapchains properly
		recreateSwapchain(wnd);
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		VK_CHECK_RESULT(result);
	}

	std::vector<VkPipelineStageFlags> waitStages = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	std::vector<VkSemaphore> waitSemaphores = { swapchain.swapchainNextImageAvailableSemaphore };

	for (size_t i = 0; i < externalWaitSemaphores.size(); i++)
	{
		waitStages.push_back(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
		waitSemaphores.push_back(externalWaitSemaphores[i]);
	}

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
	submitInfo.pWaitSemaphores = waitSemaphores.data();
	submitInfo.pWaitDstStageMask = waitStages.data();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &swapchain.swapchainCommandBuffers[imageIndex];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &swapchain.swapchainDoneRenderingSemaphore;

	if ((result = vkQueueSubmit(renderer->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE)) != VK_SUCCESS)
	{
		Log::get()->error("Failed to submit a queue, return: {}", getVkResultString(result).c_str());

		system("pause");
		//throw std::runtime_error("vulkan error - queue submit");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &swapchain.swapchainDoneRenderingSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain.swapchain;
	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(renderer->presentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		// TODO Implement out of date swapchains properly
		recreateSwapchain(wnd);
	}
	else if (result != VK_SUCCESS)
	{
		VK_CHECK_RESULT(result);
	}

	vkQueueWaitIdle(renderer->presentQueue);
}

void VulkanSwapchain::createSwapchain (Window *wnd)
{
	WindowSwapchain &swapchain = swapchains[wnd];

	swapchain.swapchainDetails = querySwapchainSupport(renderer->physicalDevice, swapchain.surface);

	VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchain.swapchainDetails.formats);
	VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchain.swapchainDetails.presentModes);
	VkExtent2D extent = chooseSwapExtent(swapchain.swapchainDetails.capabilities, wnd->getWidth(), wnd->getHeight());

	uint32_t imageCount = swapchain.swapchainDetails.capabilities.minImageCount + 1;
	if (swapchain.swapchainDetails.capabilities.maxImageCount > 0 && imageCount > swapchain.swapchainDetails.capabilities.maxImageCount)
	{
		imageCount = swapchain.swapchainDetails.capabilities.maxImageCount;
	}

	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = swapchain.surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	createInfo.preTransform = swapchain.swapchainDetails.capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = presentMode;
	createInfo.clipped = VK_TRUE;

	VK_CHECK_RESULT(vkCreateSwapchainKHR(renderer->device, &createInfo, nullptr, &swapchain.swapchain));

	vkGetSwapchainImagesKHR(renderer->device, swapchain.swapchain, &imageCount, nullptr);
	swapchain.swapchainImages.resize(imageCount);
	vkGetSwapchainImagesKHR(renderer->device, swapchain.swapchain, &imageCount, swapchain.swapchainImages.data());

	swapchain.swapchainImageFormat = surfaceFormat.format;
	swapchain.swapchainExtent = extent;

	swapchain.swapchainImageViews.resize(swapchain.swapchainImages.size());

	for (size_t i = 0; i < swapchain.swapchainImages.size(); i ++)
	{
		VkImageViewCreateInfo imageViewInfo = {};
		imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewInfo.image = swapchain.swapchainImages[i];
		imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewInfo.format = swapchain.swapchainImageFormat;
		imageViewInfo.subresourceRange =
		{	VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

		VK_CHECK_RESULT(vkCreateImageView(renderer->device, &imageViewInfo, nullptr, &swapchain.swapchainImageViews[i]));
	}

	createRenderPass(swapchain);
	createGraphicsPipeline(swapchain);
	createFramebuffers(swapchain);

	prerecordCommandBuffers(swapchain);
}

void VulkanSwapchain::createDescriptors (WindowSwapchain &swapchain)
{
	std::vector<VkDescriptorPoolSize> poolSizes = std::vector<VkDescriptorPoolSize>(2);
	poolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;
	poolSizes[0].descriptorCount = 1;
	poolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	poolSizes[1].descriptorCount = 1;

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolInfo.pPoolSizes = poolSizes.data();
	poolInfo.maxSets = 1;

	VK_CHECK_RESULT(vkCreateDescriptorPool(renderer->device, &poolInfo, nullptr, &swapchain.swapchainDescriptorPool));

	VkDescriptorSetAllocateInfo setAllocInfo = {};
	setAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocInfo.descriptorPool = swapchain.swapchainDescriptorPool;
	setAllocInfo.descriptorSetCount = 1;
	setAllocInfo.pSetLayouts = &swapchainDescriptorLayout;

	VK_CHECK_RESULT(vkAllocateDescriptorSets(renderer->device, &setAllocInfo, &swapchain.swapchainDescriptorSet));
}

void VulkanSwapchain::setSwapchainSourceImage (Window *wnd, VkImageView imageView, VkSampler sampler, VkImageLayout imageLayout, bool rewriteCommandBuffers)
{
	WindowSwapchain &swapchain = swapchains[wnd];

	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = imageLayout;
	imageInfo.imageView = imageView;
	imageInfo.sampler = sampler;

	std::vector<VkWriteDescriptorSet> writes;

	{
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = swapchain.swapchainDescriptorSet;
		writeInfo.dstBinding = 0;
		writeInfo.dstArrayElement = 0;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
		writeInfo.descriptorCount = 1;
		writeInfo.pImageInfo = &imageInfo;

		writes.push_back(writeInfo);
	}

	{
		VkWriteDescriptorSet writeInfo = {};
		writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeInfo.dstSet = swapchain.swapchainDescriptorSet;
		writeInfo.dstBinding = 1;
		writeInfo.dstArrayElement = 0;
		writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		writeInfo.descriptorCount = 1;
		writeInfo.pImageInfo = &imageInfo;

		writes.push_back(writeInfo);
	}

	vkUpdateDescriptorSets(renderer->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

	if (rewriteCommandBuffers)
		prerecordCommandBuffers(swapchain);
}

void VulkanSwapchain::prerecordCommandBuffers (WindowSwapchain &swapchain)
{
	if (swapchain.swapchainCommandBuffers.size() > 0)
	{
		vkFreeCommandBuffers(renderer->device, swapchainCommandPool, static_cast<uint32_t>(swapchain.swapchainCommandBuffers.size()), swapchain.swapchainCommandBuffers.data());
	}

	swapchain.swapchainCommandBuffers.resize(swapchain.swapchainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = swapchainCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = (uint32_t) swapchain.swapchainCommandBuffers.size();

	VK_CHECK_RESULT(vkAllocateCommandBuffers(renderer->device, &allocInfo, swapchain.swapchainCommandBuffers.data()));

	VkViewport swapchainViewport;
	VkRect2D swapchainScissor;

	swapchainViewport.x = 0.0f;
	swapchainViewport.y = (float)swapchain.swapchainExtent.height;
	swapchainViewport.width = (float) swapchain.swapchainExtent.width;
	swapchainViewport.height = -((float) swapchain.swapchainExtent.height);
	swapchainViewport.minDepth = 0.0f;
	swapchainViewport.maxDepth = 1.0f;

	swapchainScissor.offset = {0, 0};
	swapchainScissor.extent = swapchain.swapchainExtent;

	for (size_t i = 0; i < swapchain.swapchainCommandBuffers.size(); i ++)
	{
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		vkBeginCommandBuffer(swapchain.swapchainCommandBuffers[i], &beginInfo);

		debugMarkerBeginRegion(swapchain.swapchainCommandBuffers[i], "Render to Swapchain", glm::vec4(1, 0, 0, 1));

		std::vector<VkClearValue> clearValues(1);
		clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = swapchain.swapchainRenderPass;
		renderPassInfo.framebuffer = swapchain.swapchainFramebuffers[i];
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = swapchain.swapchainExtent;
		renderPassInfo.clearValueCount = 0; //static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(swapchain.swapchainCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(swapchain.swapchainCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, swapchain.swapchainPipeline);

		vkCmdBindDescriptorSets(swapchain.swapchainCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, swapchainPipelineLayout, 0, 1, &swapchain.swapchainDescriptorSet, 0, nullptr);

		vkCmdSetViewport(swapchain.swapchainCommandBuffers[i], 0, 1, &swapchainViewport);
		vkCmdSetScissor(swapchain.swapchainCommandBuffers[i], 0, 1, &swapchainScissor);

		vkCmdDraw(swapchain.swapchainCommandBuffers[i], 6, 1, 0, 0);

		vkCmdEndRenderPass(swapchain.swapchainCommandBuffers[i]);

		debugMarkerEndRegion(swapchain.swapchainCommandBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(swapchain.swapchainCommandBuffers[i]));
	}
}

void VulkanSwapchain::destroySwapchain (Window *wnd)
{
	WindowSwapchain &swapchain = swapchains[wnd];

	vkDestroyPipeline(renderer->device, swapchain.swapchainPipeline, nullptr);
	vkDestroyRenderPass(renderer->device, swapchain.swapchainRenderPass, nullptr);

	for (size_t i = 0; i < swapchain.swapchainFramebuffers.size(); i ++)
	{
		vkDestroyFramebuffer(renderer->device, swapchain.swapchainFramebuffers[i], nullptr);
	}

	for (size_t i = 0; i < swapchain.swapchainImageViews.size(); i ++)
	{
		vkDestroyImageView(renderer->device, swapchain.swapchainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(renderer->device, swapchain.swapchain, nullptr);
}

void VulkanSwapchain::createRenderPass (WindowSwapchain &swapchain)
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = swapchain.swapchainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::vector<VkAttachmentDescription> attachments = {colorAttachment};
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassCreateInfo.pAttachments = attachments.data();
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpass;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &dependency;

	VK_CHECK_RESULT(vkCreateRenderPass(renderer->device, &renderPassCreateInfo, nullptr, &swapchain.swapchainRenderPass));

	debugMarkerSetName(renderer->device, swapchain.swapchainRenderPass, VK_DEBUG_REPORT_OBJECT_TYPE_RENDER_PASS_EXT, "Swapchain Render Pass");
}

void VulkanSwapchain::createGraphicsPipeline (WindowSwapchain &swapchain)
{
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages(2);
	shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStages[0].pName = "main";
	shaderStages[0].module = swapchainVertShader;

	shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStages[1].pName = "main";
	shaderStages[1].module = swapchainFragShader;

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float) swapchain.swapchainExtent.width;
	viewport.height = (float) swapchain.swapchainExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent = swapchain.swapchainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	pipelineInfo.pDepthStencilState = nullptr;
	pipelineInfo.layout = swapchainPipelineLayout;
	pipelineInfo.renderPass = swapchain.swapchainRenderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

	VK_CHECK_RESULT(vkCreateGraphicsPipelines(renderer->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &swapchain.swapchainPipeline));

	debugMarkerSetName(renderer->device, swapchain.swapchainPipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, "Swapchain Graphics Pipeline");
}

void VulkanSwapchain::createFramebuffers (WindowSwapchain &swapchain)
{
	swapchain.swapchainFramebuffers.resize(swapchain.swapchainImageViews.size());

	for (size_t i = 0; i < swapchain.swapchainFramebuffers.size(); i ++)
	{
		std::vector<VkImageView> attachments = {swapchain.swapchainImageViews[i]};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = swapchain.swapchainRenderPass;
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();
		framebufferCreateInfo.width = swapchain.swapchainExtent.width;
		framebufferCreateInfo.height = swapchain.swapchainExtent.height;
		framebufferCreateInfo.layers = 1;

		VK_CHECK_RESULT(vkCreateFramebuffer(renderer->device, &framebufferCreateInfo, nullptr, &swapchain.swapchainFramebuffers[i]));

		debugMarkerSetName(renderer->device, swapchain.swapchainFramebuffers[i], VK_DEBUG_REPORT_OBJECT_TYPE_FRAMEBUFFER_EXT, "Swapchain Framebuffer #" + toString(i));
	}
}

void VulkanSwapchain::createSyncPrimitives (WindowSwapchain &swapchain)
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkResult result;
	if ((result = vkCreateSemaphore(renderer->device, &semaphoreInfo, nullptr, &swapchain.swapchainNextImageAvailableSemaphore)) != VK_SUCCESS)
	{
		Log::get()->error("Failed to create a semaphore, return: {}", getVkResultString(result).c_str());

		throw std::runtime_error("vulkan error - semaphore creation");
	}

	if ((result = vkCreateSemaphore(renderer->device, &semaphoreInfo, nullptr, &swapchain.swapchainDoneRenderingSemaphore)) != VK_SUCCESS)
	{
		Log::get()->error("Failed to create a semaphore, return: {}", getVkResultString(result).c_str());

		throw std::runtime_error("vulkan error - semaphore creation");
	}
}

void VulkanSwapchain::createDummySourceImage ()
{
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.extent =
	{	1, 1, 1};
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.flags = 0;

	VmaAllocationCreateInfo vmaAllocInfo = {};
	vmaAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
	vmaAllocInfo.flags = 0;

	VK_CHECK_RESULT(vmaCreateImage(renderer->memAllocator, &imageCreateInfo, &vmaAllocInfo, &swapchainDummySourceImage, &swapchainDummySourceImageMemory, nullptr));

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = swapchainDummySourceImage;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;

	VK_CHECK_RESULT(vkCreateImageView(renderer->device, &imageViewCreateInfo, nullptr, &swapchainDummySourceImageView));

	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.mipLodBias = 0;
	samplerCreateInfo.minLod = 0;
	samplerCreateInfo.maxLod = 0;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;

	VK_CHECK_RESULT(vkCreateSampler(renderer->device, &samplerCreateInfo, nullptr, &swapchainDummySampler));

	uint8_t dummyImageData[] = {0, 128, 255, 255};

	StagingBuffer stagingBuffer = renderer->createAndFillStagingBuffer(sizeof(dummyImageData), dummyImageData);

	VkCommandBuffer cmdBuffer;
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = swapchainCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	VK_CHECK_RESULT(vkAllocateCommandBuffers(renderer->device, &allocInfo, &cmdBuffer));

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &beginInfo));

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = swapchainDummySourceImage;
	barrier.subresourceRange =
	{	VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
	barrier.srcAccessMask = 0;
	barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

	VkBufferImageCopy imgCopyRegion = {};
	imgCopyRegion.bufferOffset = 0;
	imgCopyRegion.bufferRowLength = 0;
	imgCopyRegion.bufferImageHeight = 0;
	imgCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imgCopyRegion.imageSubresource.mipLevel = 0;
	imgCopyRegion.imageSubresource.baseArrayLayer = 0;
	imgCopyRegion.imageSubresource.layerCount = 1;
	imgCopyRegion.imageOffset =
	{	0, 0, 0};
	imgCopyRegion.imageExtent =
	{1, 1, 1};

	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
	vkCmdCopyBufferToImage(cmdBuffer, static_cast<VulkanStagingBuffer*> (stagingBuffer)->bufferHandle, swapchainDummySourceImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imgCopyRegion);

	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(renderer->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE))
	VK_CHECK_RESULT(vkQueueWaitIdle(renderer->graphicsQueue))

	renderer->destroyStagingBuffer(stagingBuffer);
}

void VulkanSwapchain::recreateSwapchain (Window *wnd)
{
	vkDeviceWaitIdle(renderer->device);

	destroySwapchain(wnd);
	createSwapchain(wnd);
}

SwapchainSupportDetails VulkanSwapchain::querySwapchainSupport (VkPhysicalDevice device, VkSurfaceKHR surface)
{
	SwapchainSupportDetails details;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	if (formatCount != 0)
	{
		details.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
	}

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

	if (presentModeCount != 0)
	{
		details.presentModes.resize(presentModeCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
	}

	return details;
}

VkSurfaceFormatKHR VulkanSwapchain::chooseSwapSurfaceFormat (const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
	if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		VkSurfaceFormatKHR surfaceFormat = {};
		surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
		surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

		return surfaceFormat;
	}

	for (const auto& availableFormat : availableFormats)
	{
		if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return availableFormat;
		}
	}

	return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::chooseSwapPresentMode (const std::vector<VkPresentModeKHR> availablePresentModes)
{
	return VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

	for (const auto& availablePresentMode : availablePresentModes)
	{
		if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return VK_PRESENT_MODE_MAILBOX_KHR;
		}
		else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			bestMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	return bestMode;
}

VkExtent2D VulkanSwapchain::chooseSwapExtent (const VkSurfaceCapabilitiesKHR& capabilities, uint32_t windowWidth, uint32_t windowHeight)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return capabilities.currentExtent;
	}
	else
	{
		VkExtent2D actualExtent = {windowWidth, windowHeight};

		actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
		actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

		return actualExtent;
	}
}

#endif
