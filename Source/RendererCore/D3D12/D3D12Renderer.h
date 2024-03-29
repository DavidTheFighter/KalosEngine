#ifndef RENDERING_D3D12_D3D12RENDERER_H_
#define RENDERING_D3D12_D3D12RENDERER_H_

#include <RendererCore/D3D12/d3d12_common.h>
#if BUILD_D3D12_BACKEND

#include "RendererCore/Renderer.h"

class D3D12Swapchain;
class D3D12PipelineHelper;

typedef struct
{
	D3D12_DESCRIPTOR_HEAP_TYPE heapType;
	ID3D12DescriptorHeap *heap; // If heap == nullptr, then this object hasn't allocated a d3d12 heap yet and needs to before it can be used

	uint32_t numDescriptors; // The number of descriptors allocated for this heap
	std::vector<uint8_t> allocatedDescriptors; // An array of booleans describing whether each slot of the heap has been used/allocated and if it's available
	uint32_t numFreeDescriptors; // The number of free descriptor slots
} DescriptorHeap;

class D3D12Renderer : public Renderer
{
	public:

	bool debugLayersEnabled;

	ID3D12Device2 *device;
	IDXGIAdapter4 *deviceAdapter;

	ID3D12CommandQueue *graphicsQueue;
	ID3D12CommandQueue *computeQueue;
	ID3D12CommandQueue *transferQueue;

	uint32_t cbvSrvUavDescriptorSize;
	uint32_t samplerDescriptorSize;

	std::array<DescriptorHeap, 64> massDescriptorHeaps;
	std::mutex massDescriptorHeaps_mutex;

	D3D12Renderer(const RendererAllocInfo& allocInfo);
	virtual ~D3D12Renderer();

	void createNewDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors = 1000000);

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

	Texture createTexture(suvec3 extent, ResourceFormat format, TextureUsageFlags usage, MemoryUsage memUsage, bool ownMemory, uint32_t mipLevelCount, uint32_t arrayLayerCount, uint32_t multiSampleCount);
	TextureView createTextureView(Texture texture, TextureViewType viewType, TextureSubresourceRange subresourceRange, ResourceFormat viewFormat);
	Sampler createSampler(SamplerAddressMode addressMode, SamplerFilter minFilter, SamplerFilter magFilter, float maxAnisotropy, svec3 min_max_biasLod, SamplerMipmapMode mipmapMode);

	Buffer createBuffer(size_t size, BufferUsageFlags usage, BufferLayout initialLayout, MemoryUsage memUsage, bool ownMemory);
	void *mapBuffer(Buffer buffer);
	void unmapBuffer(Buffer buffer);

	StagingBuffer createStagingBuffer(size_t dataSize);
	StagingBuffer createAndFillStagingBuffer(size_t dataSize, const void *data);
	void fillStagingBuffer(StagingBuffer stagingBuffer, size_t dataSize, const void *data);
	void *mapStagingBuffer(StagingBuffer stagingBuffer);
	void unmapStagingBuffer(StagingBuffer stagingBuffer);

	StagingTexture createStagingTexture(suvec3 extent, ResourceFormat format, uint32_t mipLevelCount, uint32_t arrayLayerCount);
	void fillStagingTextureSubresource(StagingTexture stagingTexture, const void *textureData, uint32_t mipLevel, uint32_t arrayLayer);

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
	void destroyStagingTexture(StagingTexture stagingTexture);

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

#endif
#endif /* RENDERING_D3D12_D3D12RENDERER_H_ */