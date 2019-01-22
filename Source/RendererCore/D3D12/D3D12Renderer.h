#ifndef RENDERING_D3D12_D3D12RENDERER_H_
#define RENDERING_D3D12_D3D12RENDERER_H_

#include "RendererCore/Renderer.h"

#include <RendererCore/D3D12/d3d12_common.h>

class D3D12Swapchain;
class D3D12PipelineHelper;

class D3D12Renderer : public Renderer
{
	public:

	bool debugLayersEnabled;

	ID3D12Device2 *device;
	IDXGIAdapter4 *deviceAdapter;

	ID3D12CommandQueue *graphicsQueue;
	ID3D12CommandQueue *computeQueue;
	ID3D12CommandQueue *transferQueue;

	D3D12Renderer(const RendererAllocInfo& allocInfo);
	virtual ~D3D12Renderer();

	CommandPool createCommandPool(QueueType queue, CommandPoolFlags flags);

	void submitToQueue(QueueType queue, const std::vector<CommandBuffer> &cmdBuffers, const std::vector<Semaphore> &waitSemaphores, const std::vector<PipelineStageFlags> &waitSemaphoreStages, const std::vector<Semaphore> &signalSemaphores, Fence fence);
	void waitForQueueIdle(QueueType queue);
	void waitForDeviceIdle();

	bool getFenceStatus(Fence fence);
	void resetFence(Fence fence);
	void resetFences(const std::vector<Fence> &fences);
	void waitForFence(Fence fence, double timeoutInSeconds);
	void waitForFences(const std::vector<Fence> &fences, bool waitForAll, double timeoutInSeconds);

	void writeDescriptorSets(DescriptorSet dstSet, const std::vector<DescriptorWriteInfo> &writes);

	RenderGraph createRenderGraph();
	ShaderModule createShaderModule(const std::string &file, ShaderStageFlagBits stage, ShaderSourceLanguage sourceLang, const std::string &entryPoint);
	ShaderModule createShaderModuleFromSource(const std::string &source, const std::string &referenceName, ShaderStageFlagBits stage, ShaderSourceLanguage sourceLang, const std::string &entryPoint);
	Pipeline createGraphicsPipeline(const GraphicsPipelineInfo &pipelineInfo, RenderPass renderPass, uint32_t subpass);
	Pipeline createComputePipeline(const ComputePipelineInfo &pipelineInfo);
	DescriptorPool createDescriptorPool(const DescriptorSetLayoutDescription &descriptorSetLayout, uint32_t poolBlockAllocSize);

	Fence createFence(bool createAsSignaled);
	Semaphore createSemaphore();
	std::vector<Semaphore> createSemaphores(uint32_t count);

	Texture createTexture(suvec3 extent, ResourceFormat format, TextureUsageFlags usage, MemoryUsage memUsage, bool ownMemory, uint32_t mipLevelCount, uint32_t arrayLayerCount);
	TextureView createTextureView(Texture texture, TextureViewType viewType, TextureSubresourceRange subresourceRange, ResourceFormat viewFormat);
	Sampler createSampler(SamplerAddressMode addressMode, SamplerFilter minFilter, SamplerFilter magFilter, float anisotropy, svec3 min_max_biasLod, SamplerMipmapMode mipmapMode);

	Buffer createBuffer(size_t size, BufferUsageType usage, bool canBeTransferDst, bool canBeTransferSrc, MemoryUsage memUsage, bool ownMemory);
	void *mapBuffer(Buffer buffer);
	void unmapBuffer(Buffer buffer);

	StagingBuffer createStagingBuffer(size_t dataSize);
	StagingBuffer createAndFillStagingBuffer(size_t dataSize, const void *data);
	void fillStagingBuffer(StagingBuffer stagingBuffer, size_t dataSize, const void *data);
	void *mapStagingBuffer(StagingBuffer stagingBuffer);
	void unmapStagingBuffer(StagingBuffer stagingBuffer);

	void destroyCommandPool(CommandPool pool);
	void destroyRenderGraph(RenderGraph &graph);
	void destroyPipeline(Pipeline pipeline);
	void destroyShaderModule(ShaderModule shaderModule);
	void destroyDescriptorPool(DescriptorPool pool);
	void destroyTexture(Texture texture);
	void destroyTextureView(TextureView textureView);
	void destroySampler(Sampler sampler);
	void destroyBuffer(Buffer buffer);
	void destroyStagingBuffer(StagingBuffer stagingBuffer);

	void destroyFence(Fence fence);
	void destroySemaphore(Semaphore sem);

	void setObjectDebugName(void *obj, RendererObjectType objType, const std::string &name);

	void initSwapchain(Window *wnd);
	void presentToSwapchain(Window *wnd, std::vector<Semaphore> waitSemaphores);
	void recreateSwapchain(Window *wnd);
	void setSwapchainTexture(Window *wnd, TextureView texView, Sampler sampler, TextureLayout layout);

	private:

	ID3D12Debug *debugController0;
	ID3D12Debug1 *debugController1;
	ID3D12InfoQueue *infoQueue;
	ID3D12DebugDevice *debugDevice;

	IDXGIFactory4 *dxgiFactory;

	RendererAllocInfo allocInfo;

	D3D12Swapchain *swapchainHandler;
	std::unique_ptr<D3D12PipelineHelper> pipelineHelper;

	char *temp_mapBuffer;

	void chooseDeviceAdapter();
	void createLogicalDevice();
};

#endif /* RENDERING_D3D12_D3D12RENDERER_H_ */