

#ifndef RENDERING_VULKAN_VULKANRENDERER_H_
#define RENDERING_VULKAN_VULKANRENDERER_H_

#include <RendererCore/Renderer.h>
#include <RendererCore/Vulkan/vulkan_common.h>
#include <RendererCore/Vulkan/VulkanObjects.h>

class VulkanSwapchain;
class VulkanPipelines;

class VulkanRenderer : public Renderer
{
	public:

		VkInstance instance;
		VkPhysicalDevice physicalDevice;
		VkDevice device;

		VkQueue graphicsQueue;
		VkQueue computeQueue;
		VkQueue transferQueue;
		VkQueue presentQueue;

		DeviceQueues deviceQueueInfo;
		RendererAllocInfo onAllocInfo;
		VkPhysicalDeviceFeatures deviceFeatures;
		VkPhysicalDeviceProperties deviceProps;

		VmaAllocator memAllocator;

#ifdef __linux__
		shaderc::Compiler *defaultCompiler;
#endif
		VulkanSwapchain *swapchains;
		VulkanPipelines *pipelineHandler; // Does all the pipeline handling (creation, caching, etc)

		VulkanRenderer (const RendererAllocInfo& allocInfo);
		virtual ~VulkanRenderer ();

		CommandPool createCommandPool (QueueType queue, CommandPoolFlags flags);

		void submitToQueue (QueueType queue, const std::vector<CommandBuffer> &cmdBuffers, const std::vector<Semaphore> &waitSemaphores, const std::vector<PipelineStageFlags> &waitSemaphoreStages, const std::vector<Semaphore> &signalSemaphores, Fence fence);
		void waitForQueueIdle (QueueType queue);
		void waitForDeviceIdle ();

		bool getFenceStatus (Fence fence);
		void resetFence (Fence fence);
		void resetFences (const std::vector<Fence> &fences);
		void waitForFence (Fence fence, double timeoutInSeconds);
		void waitForFences (const std::vector<Fence> &fences, bool waitForAll, double timeoutInSeconds);

		void writeDescriptorSets (const std::vector<DescriptorWriteInfo> &writes);

		RenderPass createRenderPass(const std::vector<AttachmentDescription> &attachments, const std::vector<SubpassDescription> &subpasses, const std::vector<SubpassDependency> &dependencies);
		RenderGraph createRenderGraph();
		Framebuffer createFramebuffer(RenderPass renderPass, const std::vector<TextureView> &attachments, uint32_t width, uint32_t height, uint32_t layers);
		ShaderModule createShaderModule (const std::string &file, ShaderStageFlagBits stage, ShaderSourceLanguage sourceLang, const std::string &entryPoint);
		ShaderModule createShaderModuleFromSource (const std::string &source, const std::string &referenceName, ShaderStageFlagBits stage, ShaderSourceLanguage sourceLang, const std::string &entryPoint);
		Pipeline createGraphicsPipeline (const GraphicsPipelineInfo &pipelineInfo, RenderPass renderPass, uint32_t subpass);
		Pipeline createComputePipeline(const ComputePipelineInfo &pipelineInfo);
		DescriptorPool createDescriptorPool (const std::vector<DescriptorSetLayoutBinding> &layoutBindings, uint32_t poolBlockAllocSize);

		Fence createFence (bool createAsSignaled);
		Semaphore createSemaphore ();
		std::vector<Semaphore> createSemaphores (uint32_t count);

		Texture createTexture (suvec3 extent, ResourceFormat format, TextureUsageFlags usage, MemoryUsage memUsage, bool ownMemory, uint32_t mipLevelCount, uint32_t arrayLayerCount);
		TextureView createTextureView (Texture texture, TextureViewType viewType, TextureSubresourceRange subresourceRange, ResourceFormat viewFormat);
		Sampler createSampler (SamplerAddressMode addressMode, SamplerFilter minFilter, SamplerFilter magFilter, float anisotropy, svec3 min_max_biasLod, SamplerMipmapMode mipmapMode);

		Buffer createBuffer (size_t size, BufferUsageType usage, bool canBeTransferDst, bool canBeTransferSrc, MemoryUsage memUsage, bool ownMemory);
		void *mapBuffer (Buffer buffer);
		void unmapBuffer (Buffer buffer);

		StagingBuffer createStagingBuffer (size_t dataSize);
		StagingBuffer createAndFillStagingBuffer (size_t dataSize, const void *data);
		void fillStagingBuffer (StagingBuffer stagingBuffer, size_t dataSize, const void *data);
		void *mapStagingBuffer(StagingBuffer stagingBuffer);
		void unmapStagingBuffer(StagingBuffer stagingBuffer);

		void destroyCommandPool (CommandPool pool);
		void destroyRenderGraph(RenderGraph &graph);
		void destroyPipeline (Pipeline pipeline);
		void destroyShaderModule (ShaderModule module);
		void destroyDescriptorPool (DescriptorPool pool);
		void destroyTexture (Texture texture);
		void destroyTextureView (TextureView textureView);
		void destroySampler (Sampler sampler);
		void destroyBuffer (Buffer buffer);
		void destroyStagingBuffer (StagingBuffer stagingBuffer);

		void destroyFence (Fence fence);
		void destroySemaphore (Semaphore sem);

		void setObjectDebugName (void *obj, RendererObjectType objType, const std::string &name);

		void initSwapchain (Window *wnd);
		void presentToSwapchain (Window *wnd);
		void recreateSwapchain (Window *wnd);
		void setSwapchainTexture (Window *wnd, TextureView texView, Sampler sampler, TextureLayout layout);

		bool areValidationLayersEnabled ();

		bool checkExtraSurfacePresentSupport(VkSurfaceKHR surface);

		VulkanDescriptorPoolObject createDescPoolObject (const std::vector<VkDescriptorPoolSize> &poolSizes, uint32_t maxSets);

	private:


		VkDebugReportCallbackEXT dbgCallback;

		bool validationLayersEnabled;

		void choosePhysicalDevice ();
		void createLogicalDevice ();

		void cleanupVulkan ();

		DeviceQueues findQueueFamilies (VkPhysicalDevice physDevice);
		std::vector<const char*> getInstanceExtensions ();

		bool checkValidationLayerSupport (std::vector<const char*> layers);
		bool checkDeviceExtensionSupport (VkPhysicalDevice physDevice, std::vector<const char*> extensions);

		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback (VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);
};

#endif /* RENDERING_VULKAN_VULKANRENDERER_H_ */
