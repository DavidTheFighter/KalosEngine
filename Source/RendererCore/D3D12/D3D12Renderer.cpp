#include "RendererCore/D3D12/D3D12Renderer.h"
#if BUILD_D3D12_BACKEND

#include <RendererCore/D3D12/D3D12CommandPool.h>
#include <RendererCore/D3D12/D3D12DescriptorPool.h>
#include <RendererCore/D3D12/D3D12Enums.h>
#include <RendererCore/D3D12/D3D12Objects.h>

#include <RendererCore/D3D12/D3D12Swapchain.h>
#include <RendererCore/D3D12/D3D12PipelineHelper.h>
#include <RendererCore/D3D12/D3D12RenderGraph.h>

#include <Resources/FileLoader.h>

D3D12Renderer::D3D12Renderer(const RendererAllocInfo& allocInfo)
{
	this->allocInfo = allocInfo;

	debugController0 = nullptr;
	debugController1 = nullptr;
	infoQueue;
	device = nullptr;

	swapchainHandler = nullptr;
	debugLayersEnabled = false;
	temp_mapBuffer = new char[64 * 1024 * 1024];

	if (std::find(allocInfo.launchArgs.begin(), allocInfo.launchArgs.end(), "-enable_d3d12_debug") != allocInfo.launchArgs.end())
	{
		debugLayersEnabled = true;

		Log::get()->info("Enabling debug layers for the D3D12 backend");
	}

	if (debugLayersEnabled)
	{
		DX_CHECK_RESULT(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController0)));
		DX_CHECK_RESULT(debugController0->QueryInterface(IID_PPV_ARGS(&debugController1)));

		if (std::find(allocInfo.launchArgs.begin(), allocInfo.launchArgs.end(), "-enable_d3d12_hw_debug") != allocInfo.launchArgs.end())
		{
			debugController1->SetEnableGPUBasedValidation(true);

			Log::get()->warn("Prepare to wait a while, D3D12 GPU/Hardware based validation is enabled");
		}

		debugController1->EnableDebugLayer();
	}

	uint32_t createFactoryFlags = 0;

	if (debugLayersEnabled)
		createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

	DX_CHECK_RESULT(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	chooseDeviceAdapter();
	createLogicalDevice();

	if (debugLayersEnabled)
	{
		device->QueryInterface(IID_PPV_ARGS(&debugDevice));
	}

	// Setup the d3d12 debug layer filter & stuff
	if (debugLayersEnabled && false)
	{
		DX_CHECK_RESULT(device->QueryInterface(IID_PPV_ARGS(&infoQueue)));

		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, FALSE);

		std::vector<D3D12_MESSAGE_SEVERITY> severities = {D3D12_MESSAGE_SEVERITY_INFO};
		std::vector<D3D12_MESSAGE_ID> denyIDs = {};

		D3D12_INFO_QUEUE_FILTER dbgFilter = {};
		dbgFilter.DenyList.NumSeverities = (UINT) severities.size();
		dbgFilter.DenyList.pSeverityList = severities.data();
		dbgFilter.DenyList.NumIDs = (UINT) denyIDs.size();
		dbgFilter.DenyList.pIDList = denyIDs.data();

		DX_CHECK_RESULT(infoQueue->PushStorageFilter(&dbgFilter));
	}

	swapchainHandler = new D3D12Swapchain(this);
	initSwapchain(allocInfo.mainWindow);

	pipelineHelper = std::unique_ptr<D3D12PipelineHelper>(new D3D12PipelineHelper(this));

	cbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	samplerDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	for (size_t i = 0; i < massDescriptorHeaps.size(); i++)
		massDescriptorHeaps[i].heap = nullptr;

	createNewDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	createNewDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
}

D3D12Renderer::~D3D12Renderer()
{
	for (size_t i = 0; i < massDescriptorHeaps.size(); i++)
		if (massDescriptorHeaps[i].heap != nullptr)
			massDescriptorHeaps[i].heap->Release();

	delete swapchainHandler;

	graphicsQueue->Release();
	computeQueue->Release();
	transferQueue->Release();
	
	if (debugLayersEnabled && debugDevice)
	{
		debugDevice->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);

		debugDevice->Release();

		debugController0->Release();
		debugController1->Release();
	}

	deviceAdapter->Release();
	device->Release();

	dxgiFactory->Release();

	delete temp_mapBuffer;
}

void D3D12Renderer::chooseDeviceAdapter()
{
	uint32_t adapterIndex = 0;
	IDXGIAdapter1 *adapter1 = nullptr;
	size_t maxDedicatedVideoMemory = 0;

	while (dxgiFactory->EnumAdapters1(adapterIndex, &adapter1) != DXGI_ERROR_NOT_FOUND)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		adapter1->GetDesc1(&dxgiAdapterDesc);
				
		bool isNotSoftware = (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0;
		
		if (isNotSoftware && SUCCEEDED(D3D12CreateDevice(adapter1, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)) && dxgiAdapterDesc.DedicatedVideoMemory > maxDedicatedVideoMemory)
		{
			maxDedicatedVideoMemory = dxgiAdapterDesc.DedicatedVideoMemory;
			adapter1->QueryInterface(&deviceAdapter);
		}

		adapter1->Release();
		adapterIndex++;
	}
}

void D3D12Renderer::createLogicalDevice()
{
	DX_CHECK_RESULT(D3D12CreateDevice(deviceAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)));

	D3D12_COMMAND_QUEUE_DESC gfxQueueDesc = {};
	gfxQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	gfxQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	gfxQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	gfxQueueDesc.NodeMask = 0;

	D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
	computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	computeQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	computeQueueDesc.NodeMask = 0;

	D3D12_COMMAND_QUEUE_DESC transferQueueDesc = {};
	transferQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	transferQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	transferQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	transferQueueDesc.NodeMask = 0;

	DX_CHECK_RESULT(device->CreateCommandQueue(&gfxQueueDesc, IID_PPV_ARGS(&graphicsQueue)));
	DX_CHECK_RESULT(device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&computeQueue)));
	DX_CHECK_RESULT(device->CreateCommandQueue(&transferQueueDesc, IID_PPV_ARGS(&transferQueue)));
}

void D3D12Renderer::createNewDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t numDescriptors)
{
	if (heapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
		numDescriptors = std::min<uint32_t>(numDescriptors, 2048);
	else
		numDescriptors = std::min<uint32_t>(numDescriptors, 1000000);

	std::lock_guard<std::mutex> lockGuard(massDescriptorHeaps_mutex);

	for (size_t i = 0; i < massDescriptorHeaps.size(); i++)
	{
		DescriptorHeap &descHeap = massDescriptorHeaps[i];

		if (descHeap.heap == nullptr)
		{
			D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
			descHeapDesc.Type = heapType;
			descHeapDesc.NumDescriptors = numDescriptors;
			descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			descHeapDesc.NodeMask = 0;

			device->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&descHeap.heap));

			descHeap.heapType = heapType;
			descHeap.numDescriptors = numDescriptors;
			descHeap.allocatedDescriptors = std::vector<uint8_t>(numDescriptors, 0);
			descHeap.numFreeDescriptors = numDescriptors;

			break;
		}
	}
}

CommandPool D3D12Renderer::createCommandPool(QueueType queue, CommandPoolFlags flags)
{
	D3D12CommandPool *cmdPool = new D3D12CommandPool(this, queue);

	return cmdPool;
}

void D3D12Renderer::submitToQueue(QueueType queueType, const std::vector<CommandBuffer>& cmdBuffers, const std::vector<Semaphore>& waitSemaphores, const std::vector<PipelineStageFlags>& waitSemaphoreStages, const std::vector<Semaphore>& signalSemaphores, Fence fence)
{
	ID3D12CommandQueue *cmdQueue = nullptr;

	switch (queueType)
	{
		case QUEUE_TYPE_COMPUTE:
			cmdQueue = computeQueue;
			break;
		case QUEUE_TYPE_TRANSFER:
			cmdQueue = transferQueue;
			break;
		case QUEUE_TYPE_GRAPHICS:
		default:
			cmdQueue = graphicsQueue;
	}

	std::vector<ID3D12CommandList*> cmdLists;

	for (CommandBuffer cmdBuffer : cmdBuffers)
		cmdLists.push_back(static_cast<D3D12CommandBuffer*>(cmdBuffer)->cmdList);

	for (Semaphore sem : waitSemaphores)
	{
		D3D12Semaphore *d3dsem = static_cast<D3D12Semaphore*>(sem);

		if (!d3dsem->pendingWait)
		{
			Log::get()->error("D3D12Renderer: Cannot wait on a semaphore that hasn't been used as a signal sempahore, signal it as a signal semaphore before waiting on it");

			continue;
		}

		d3dsem->pendingWait = false;
		
		cmdQueue->Wait(d3dsem->semFence, d3dsem->semFenceValue);
	}

	cmdQueue->ExecuteCommandLists(cmdLists.size(), cmdLists.data());
	
	for (Semaphore sem : signalSemaphores)
	{
		D3D12Semaphore *d3dsem = static_cast<D3D12Semaphore*>(sem);

		if (d3dsem->pendingWait)
		{
			Log::get()->error("D3D12Renderer: Cannot signal a sempahore that has already been signaled/not been waited on, use it/wait on it before signaling again");

			continue;
		}

		d3dsem->pendingWait = true;
		UINT64 d3dsemWaitValue = ++d3dsem->semFenceValue;

		cmdQueue->Signal(d3dsem->semFence, d3dsemWaitValue);

		if (d3dsem->semFence->GetCompletedValue() < d3dsemWaitValue)
			DX_CHECK_RESULT(d3dsem->semFence->SetEventOnCompletion(d3dsemWaitValue, d3dsem->semFenceWaitEvent));
	}

	if (fence != nullptr)
	{
		D3D12Fence *d3dfence = static_cast<D3D12Fence*>(fence);

		DEBUG_ASSERT(d3dfence->unsignaled);

		UINT64 d3dfenceWaitValue = ++d3dfence->fenceValue;

		cmdQueue->Signal(d3dfence->fence, d3dfenceWaitValue);

		if (d3dfence->fence->GetCompletedValue() < d3dfenceWaitValue)
			DX_CHECK_RESULT(d3dfence->fence->SetEventOnCompletion(d3dfenceWaitValue, d3dfence->fenceEvent));

		d3dfence->unsignaled = false;
	}
}

void D3D12Renderer::waitForQueueIdle(QueueType queue)
{
}

void D3D12Renderer::waitForDeviceIdle()
{
}

bool D3D12Renderer::getFenceStatus(Fence fence)
{
	return false;
}

void D3D12Renderer::resetFence(Fence fence)
{
}

void D3D12Renderer::resetFences(const std::vector<Fence>& fences)
{
}

void D3D12Renderer::waitForFence(Fence fence, double timeoutInSeconds)
{
	D3D12Fence *d3dfence = static_cast<D3D12Fence*>(fence);

	if (d3dfence->unsignaled)
	{
		Log::get()->error("D3D12Renderer: Cannot wait on an unsignaled fence");

		return;
	}

	WaitForSingleObject(d3dfence->fenceEvent, static_cast<DWORD>(timeoutInSeconds * 1000.0));
}

void D3D12Renderer::waitForFences(const std::vector<Fence>& fences, bool waitForAll, double timeoutInSeconds)
{
	std::vector<HANDLE> waitHandles;

	for (size_t i = 0; i < fences.size(); i++)
	{
		D3D12Fence *d3dfence = static_cast<D3D12Fence*>(fences[i]);

		if (d3dfence->unsignaled)
		{
			Log::get()->error("D3D12Renderer: Cannot wait on an unsignaled fence");

			 continue;
		}

		waitHandles.push_back(d3dfence->fenceEvent);
	}

	WaitForMultipleObjects((DWORD)waitHandles.size(), waitHandles.data(), waitForAll, static_cast<DWORD>(timeoutInSeconds * 1000.0));
}

inline D3D12_SHADER_RESOURCE_VIEW_DESC createSRVDescFromTextureView(TextureView view)
{
	D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = {};
	viewDesc.Format = ResourceFormatToDXGIFormat(view->viewFormat);
	viewDesc.ViewDimension = textureViewTypeToD3D12SRVDimension(view->viewType);
	viewDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	
	switch (viewDesc.ViewDimension)
	{
		case D3D12_SRV_DIMENSION_TEXTURE1D:
		{
			D3D12_TEX1D_SRV tex = {};
			tex.MostDetailedMip = view->baseMip;
			tex.MipLevels = view->mipCount;
			tex.ResourceMinLODClamp = 0.0f;

			viewDesc.Texture1D = tex;
			break;
		}
		case D3D12_SRV_DIMENSION_TEXTURE1DARRAY:
		{
			D3D12_TEX1D_ARRAY_SRV tex = {};
			tex.MostDetailedMip = view->baseMip;
			tex.MipLevels = view->mipCount;
			tex.FirstArraySlice = view->baseLayer;
			tex.ArraySize = view->layerCount;
			tex.ResourceMinLODClamp = 0.0f;

			viewDesc.Texture1DArray = tex;
			break;
		}
		case D3D12_SRV_DIMENSION_TEXTURE2D:
		{
			D3D12_TEX2D_SRV tex = {};
			tex.MostDetailedMip = view->baseMip;
			tex.MipLevels = view->mipCount;
			tex.PlaneSlice = 0;
			tex.ResourceMinLODClamp = 0.0f;

			viewDesc.Texture2D = tex;
			break;
		}
		case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
		{
			D3D12_TEX2D_ARRAY_SRV tex = {};
			tex.MostDetailedMip = view->baseMip;
			tex.MipLevels = view->mipCount;
			tex.FirstArraySlice = view->baseLayer;
			tex.ArraySize = view->layerCount;
			tex.PlaneSlice = 0;
			tex.ResourceMinLODClamp = 0.0f;

			viewDesc.Texture2DArray = tex;
			break;
		}
		case D3D12_SRV_DIMENSION_TEXTURE3D:
		{
			D3D12_TEX3D_SRV tex = {};
			tex.MostDetailedMip = view->baseMip;
			tex.MipLevels = view->mipCount;
			tex.ResourceMinLODClamp = 0.0f;

			viewDesc.Texture3D = tex;
			break;
		}
		case D3D12_SRV_DIMENSION_TEXTURECUBE:
		{
			D3D12_TEXCUBE_SRV tex = {};
			tex.MostDetailedMip = view->baseMip;
			tex.MipLevels = view->mipCount;
			tex.ResourceMinLODClamp = 0.0f;

			viewDesc.TextureCube = tex;
			break;
		}
		case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
		{
			D3D12_TEXCUBE_ARRAY_SRV tex = {};
			tex.MostDetailedMip = view->baseMip;
			tex.MipLevels = view->mipCount;
			tex.First2DArrayFace = view->baseLayer;
			tex.NumCubes = view->layerCount / 6;
			tex.ResourceMinLODClamp = 0.0f;

			viewDesc.TextureCubeArray = tex;
			break;
		}
		default:
		{
			D3D12_TEX2D_SRV tex = {};
			tex.MostDetailedMip = 0;
			tex.MipLevels = 1;
			tex.PlaneSlice = 0;
			tex.ResourceMinLODClamp = 0.0f;

			viewDesc.Texture2D = tex;
		}
	}

	return viewDesc;
}

inline D3D12_UNORDERED_ACCESS_VIEW_DESC createUAVDescFromTextureView(TextureView view)
{
	D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
	viewDesc.Format = ResourceFormatToDXGIFormat(view->viewFormat);
	viewDesc.ViewDimension = textureViewTypeToD3D12UAVDimension(view->viewType);

	switch (view->viewType)
	{
		case TEXTURE_VIEW_TYPE_1D:
		{
			D3D12_TEX1D_UAV tex = {};
			tex.MipSlice = view->baseMip;

			viewDesc.Texture1D = tex;
			break;
		}
		case TEXTURE_VIEW_TYPE_1D_ARRAY:
		{
			D3D12_TEX1D_ARRAY_UAV tex = {};
			tex.MipSlice = view->baseMip;
			tex.FirstArraySlice = view->baseLayer;
			tex.ArraySize = view->layerCount;

			viewDesc.Texture1DArray = tex;
			break;
		}
		case TEXTURE_VIEW_TYPE_2D:
		{
			D3D12_TEX2D_UAV tex = {};
			tex.MipSlice = view->baseMip;
			tex.PlaneSlice = 0;

			viewDesc.Texture2D = tex;
			break;
		}
		case TEXTURE_VIEW_TYPE_2D_ARRAY:
		case TEXTURE_VIEW_TYPE_CUBE:
		case TEXTURE_VIEW_TYPE_CUBE_ARRAY:
		{
			D3D12_TEX2D_ARRAY_UAV tex = {};
			tex.MipSlice = view->baseMip;
			tex.FirstArraySlice = view->baseLayer;
			tex.ArraySize = view->layerCount;
			tex.PlaneSlice = 0;

			viewDesc.Texture2DArray = tex;
			break;
		}
		case TEXTURE_VIEW_TYPE_3D:
		{
			D3D12_TEX3D_UAV tex = {};
			tex.MipSlice = view->baseMip;
			tex.FirstWSlice = 0;
			tex.WSize = view->parentTexture->depth;

			viewDesc.Texture3D = tex;
			break;
		}
		default:
		{
			D3D12_TEX2D_UAV tex = {};
			tex.MipSlice = 0;
			tex.PlaneSlice = 0;

			viewDesc.Texture2D = tex;
			break;
		}
	}

	return viewDesc;
}

void D3D12Renderer::writeDescriptorSets(DescriptorSet dstSet, const std::vector<DescriptorWriteInfo>& writes)
{
	D3D12DescriptorSet *d3dset = static_cast<D3D12DescriptorSet*>(dstSet);

	uint32_t constantBufferOffset = 0;
	uint32_t inputAttachmentOffset = constantBufferOffset + d3dset->constantBufferCount;
	uint32_t storageBufferOffset = inputAttachmentOffset + d3dset->inputAtttachmentCount;
	uint32_t storageTextureOffset = storageBufferOffset + d3dset->storageBufferCount;
	uint32_t sampledTextureOffset = storageTextureOffset + d3dset->storageTextureCount;

	for (size_t i = 0; i < writes.size(); i++)
	{
		const DescriptorWriteInfo &writeInfo = writes[i];
		
		switch (writeInfo.descriptorType)
		{
			case DESCRIPTOR_TYPE_SAMPLER:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descHandle = d3dset->samplerHeap->GetCPUDescriptorHandleForHeapStart();
				descHandle.ptr += samplerDescriptorSize * (writeInfo.dstBinding + d3dset->samplerStartDescriptorSlot);

				device->CreateSampler(&static_cast<D3D12Sampler*>(writeInfo.samplerInfo.sampler)->samplerDesc, descHandle);

				break;
			}
			case DESCRIPTOR_TYPE_CONSTANT_BUFFER:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descHandle = d3dset->srvUavCbvHeap->GetCPUDescriptorHandleForHeapStart();
				descHandle.ptr += cbvSrvUavDescriptorSize * (writeInfo.dstBinding + d3dset->srvUavCbvStartDescriptorSlot + constantBufferOffset);

				D3D12_CONSTANT_BUFFER_VIEW_DESC viewDesc = {};
				viewDesc.BufferLocation = static_cast<D3D12Buffer*>(writeInfo.bufferInfo.buffer)->bufferResource->GetGPUVirtualAddress() + writeInfo.bufferInfo.offset;
				viewDesc.SizeInBytes = (writeInfo.bufferInfo.range + 255) & (~255);

				device->CreateConstantBufferView(&viewDesc, descHandle);

				break;
			}
			case DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descHandle = d3dset->srvUavCbvHeap->GetCPUDescriptorHandleForHeapStart();
				descHandle.ptr += cbvSrvUavDescriptorSize * (writeInfo.dstBinding + d3dset->srvUavCbvStartDescriptorSlot + inputAttachmentOffset);

				D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = createSRVDescFromTextureView(writeInfo.inputAttachmentInfo.view);

				device->CreateShaderResourceView(static_cast<D3D12Texture*>(writeInfo.inputAttachmentInfo.view->parentTexture)->textureResource, &viewDesc, descHandle);

				break;
			}
			case DESCRIPTOR_TYPE_STORAGE_BUFFER:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descHandle = d3dset->srvUavCbvHeap->GetCPUDescriptorHandleForHeapStart();
				descHandle.ptr += cbvSrvUavDescriptorSize * (writeInfo.dstBinding + d3dset->srvUavCbvStartDescriptorSlot + storageBufferOffset);

				D3D12_BUFFER_UAV buffer = {};
				buffer.FirstElement = writeInfo.bufferInfo.offset / 4;
				buffer.NumElements = writeInfo.bufferInfo.range / 4;
				buffer.StructureByteStride = 0;
				buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;

				D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = {};
				viewDesc.Format = DXGI_FORMAT_R32_TYPELESS;
				viewDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				viewDesc.Buffer = buffer;

				device->CreateUnorderedAccessView(static_cast<D3D12Buffer*>(writeInfo.bufferInfo.buffer)->bufferResource, nullptr, &viewDesc, descHandle);

				break;
			}
			case DESCRIPTOR_TYPE_STORAGE_TEXTURE:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descHandle = d3dset->srvUavCbvHeap->GetCPUDescriptorHandleForHeapStart();
				descHandle.ptr += cbvSrvUavDescriptorSize * (writeInfo.dstBinding + d3dset->srvUavCbvStartDescriptorSlot + storageTextureOffset);

				D3D12_UNORDERED_ACCESS_VIEW_DESC viewDesc = createUAVDescFromTextureView(writeInfo.sampledTextureInfo.view);

				device->CreateUnorderedAccessView(static_cast<D3D12Texture*>(writeInfo.sampledTextureInfo.view->parentTexture)->textureResource, nullptr, &viewDesc, descHandle);

				break;
			}
			case DESCRIPTOR_TYPE_SAMPLED_TEXTURE:
			{
				D3D12_CPU_DESCRIPTOR_HANDLE descHandle = d3dset->srvUavCbvHeap->GetCPUDescriptorHandleForHeapStart();
				descHandle.ptr += cbvSrvUavDescriptorSize * (writeInfo.dstBinding + d3dset->srvUavCbvStartDescriptorSlot + sampledTextureOffset);

				D3D12_SHADER_RESOURCE_VIEW_DESC viewDesc = createSRVDescFromTextureView(writeInfo.sampledTextureInfo.view);

				device->CreateShaderResourceView(static_cast<D3D12Texture*>(writeInfo.sampledTextureInfo.view->parentTexture)->textureResource, &viewDesc, descHandle);

				break;
			}
		}
	}
}

RenderGraph D3D12Renderer::createRenderGraph()
{
	D3D12RenderGraph *renderGraph = new D3D12RenderGraph(this);

	return renderGraph;
}

ShaderModule D3D12Renderer::createShaderModule(const std::string &file, ShaderStageFlagBits stage, ShaderSourceLanguage sourceLang, const std::string &entryPoint)
{
	std::string source = FileLoader::instance()->readFile(file);

	size_t i = file.find("/shaders/");

	std::string debugMarkerName = file;

	if (i != file.npos)
		debugMarkerName = ".../" + file.substr(i + 9);

	return createShaderModuleFromSource(source, debugMarkerName, stage, sourceLang, entryPoint);
}

ShaderModule D3D12Renderer::createShaderModuleFromSource(const std::string &source, const std::string &referenceName, ShaderStageFlagBits stage, ShaderSourceLanguage sourceLang, const std::string &entryPoint)
{
	D3D12ShaderModule *shaderModule = new D3D12ShaderModule();
	ID3DBlob *blob = nullptr, *errorBuf = nullptr;

	const D3D_SHADER_MACRO macroDefines[] = {
		{nullptr, nullptr}
	};

	std::string modSource = source;
	modSource = std::string(D3D12_HLSL_SAMPLED_TEXTURE_PREPROCESSOR) + "\n" + modSource;
	modSource = std::string(D3D12_HLSL_STORAGE_TEXTURE_PREPROCESSOR) + "\n" + modSource;
	modSource = std::string(D3D12_HLSL_STORAGE_BUFFER_PREPROCESSOR) + "\n" + modSource;
	modSource = std::string(D3D12_HLSL_INPUT_ATTACHMENT_PREPROCESSOR) + "\n" + modSource;
	modSource = std::string(D3D12_HLSL_CBUFFER_PREPROCESSOR) + "\n" + modSource;
	modSource = std::string(D3D12_HLSL_SAMPLER_PREPROCESSOR) + "\n" + modSource;
	modSource = std::string(D3D12_HLSL_PUSHCONSTANTBUFFER_PREPROCESSOR) + "\n" + modSource;
	modSource = std::string(D3D12_HLSL_MERGE_PREPROCESSOR) + "\n" + modSource;

	HRESULT hr = D3DCompile(modSource.data(), modSource.size(), referenceName.c_str(), macroDefines, nullptr, entryPoint.c_str(), shaderStageFlagBitsToD3D12CompilerTargetStr(stage).c_str(), D3DCOMPILE_DEBUG | D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &blob, &errorBuf);

	if (FAILED(hr))
	{
		Log::get()->error("D3D12 failed to compile a shader, error msg: {}", std::string((char*) errorBuf->GetBufferPointer()));
		throw std::runtime_error("d3d12 shader compile error");
	}

	shaderModule->shaderBytecode = std::unique_ptr<char>(new char[blob->GetBufferSize()]);
	memcpy(shaderModule->shaderBytecode.get(), blob->GetBufferPointer(), blob->GetBufferSize());

	shaderModule->shaderBytecodeLength = blob->GetBufferSize();
	shaderModule->stage = stage;
	shaderModule->entryPoint = entryPoint;

	blob->Release();

	if (errorBuf != nullptr)
		errorBuf->Release();

	return shaderModule;
}

Pipeline D3D12Renderer::createGraphicsPipeline(const GraphicsPipelineInfo & pipelineInfo, RenderPass renderPass, uint32_t subpass)
{
	return pipelineHelper->createGraphicsPipeline(pipelineInfo, renderPass, subpass);
}

Pipeline D3D12Renderer::createComputePipeline(const ComputePipelineInfo &pipelineInfo)
{
	return pipelineHelper->createComputePipeline(pipelineInfo);
}

DescriptorPool D3D12Renderer::createDescriptorPool(const DescriptorSetLayoutDescription &descriptorSetLayout, uint32_t poolBlockAllocSize)
{
	D3D12DescriptorPool *pool = new D3D12DescriptorPool(this, descriptorSetLayout);

	return pool;
}

Fence D3D12Renderer::createFence(bool createAsSignaled)
{
	D3D12Fence *fence = new D3D12Fence();
	fence->fenceValue = 0;
	fence->fenceEvent = createEventHandle();
	fence->unsignaled = true;

	DX_CHECK_RESULT(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence->fence)));

	return fence;
}

Semaphore D3D12Renderer::createSemaphore()
{
	return createSemaphores(1)[0];
}

std::vector<Semaphore> D3D12Renderer::createSemaphores(uint32_t count)
{
	std::vector<Semaphore> sems;

	for (uint32_t i = 0; i < count; i++)
	{
		D3D12Semaphore *sem = new D3D12Semaphore();
		sem->semFenceValue = 0;
		sem->semFenceWaitEvent = createEventHandle();
		sem->pendingWait = false;

		DX_CHECK_RESULT(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&sem->semFence)));

		sems.push_back(sem);
	}

	return sems;
}

Texture D3D12Renderer::createTexture(suvec3 extent, ResourceFormat format, TextureUsageFlags usage, MemoryUsage memUsage, bool ownMemory, uint32_t mipLevelCount, uint32_t arrayLayerCount)
{
#if D3D12_DEBUG_COMPATIBILITY_CHECKS
	DEBUG_ASSERT(!(extent.z > 1 && arrayLayerCount > 1) && "3D Texture arrays are not supported, only one of extent.z or arrayLayerCount can be greater than 1");

	if ((format == RESOURCE_FORMAT_A2R10G10B10_UINT_PACK32 || format == RESOURCE_FORMAT_A2R10G10B10_UNORM_PACK32) && (usage & TEXTURE_USAGE_TRANSFER_DST_BIT))
		Log::get()->warn("There are no swizzle equivalents between D3D12 and Vulkan, using R10G10B10A2 and A2R10G10B10, respectively, and the renderer backend does not convert the texture data for you");

	if (format == RESOURCE_FORMAT_B10G11R11_UFLOAT_PACK32 && (usage & TEXTURE_USAGE_TRANSFER_DST_BIT))
		Log::get()->warn("There are no swizzle equivalents between D3D12 and Vulkan, using R11G11B10 and B10G11R11, respectively, and the renderer backend does not convert the texture data for you");

	if (format == RESOURCE_FORMAT_E5B9G9R9_UFLOAT_PACK32 && (usage & TEXTURE_USAGE_TRANSFER_DST_BIT))
		Log::get()->warn("There are no swizzle equivalents between D3D12 and Vulkan, using R9G9B9E5 and E5B9G9R9, respectively, and the renderer backend does not convert the texture data for you");

#endif

	D3D12Texture *texture = new D3D12Texture();
	texture->width = (uint32_t) extent.x;
	texture->height = (uint32_t) extent.y;
	texture->depth = (uint32_t) extent.z;
	texture->textureFormat = format;
	texture->layerCount = arrayLayerCount;
	texture->mipCount = mipLevelCount;

	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = extent.z > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : (extent.y > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE2D : D3D12_RESOURCE_DIMENSION_TEXTURE1D);
	texDesc.Alignment = 0;
	texDesc.Width = extent.x;
	texDesc.Height = extent.y;
	texDesc.DepthOrArraySize = std::max<uint32_t>(extent.z, arrayLayerCount);
	texDesc.MipLevels = mipLevelCount;
	texDesc.Format = ResourceFormatToDXGIFormat(format);
	texDesc.SampleDesc = {1, 0};
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (usage & TEXTURE_USAGE_COLOR_ATTACHMENT_BIT)
		texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	if (usage & TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// TODO Apply D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE where applicable

	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;

	switch (memUsage)
	{
		case MEMORY_USAGE_GPU_ONLY:
			heapType = D3D12_HEAP_TYPE_DEFAULT;
			break;
		case MEMORY_USAGE_CPU_ONLY:
		case MEMORY_USAGE_CPU_TO_GPU:
			heapType = D3D12_HEAP_TYPE_UPLOAD;
			break;
	}

	DX_CHECK_RESULT(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(heapType), D3D12_HEAP_FLAG_NONE, &texDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&texture->textureResource)));

	return texture;
}

TextureView D3D12Renderer::createTextureView(Texture texture, TextureViewType viewType, TextureSubresourceRange subresourceRange, ResourceFormat viewFormat)
{
	D3D12TextureView *textureView = new D3D12TextureView();
	textureView->parentTexture = texture;
	textureView->viewType = viewType;
	textureView->viewFormat = viewFormat == RESOURCE_FORMAT_UNDEFINED ? texture->textureFormat : viewFormat;
	textureView->baseMip = subresourceRange.baseMipLevel;
	textureView->mipCount = subresourceRange.levelCount;
	textureView->baseLayer = subresourceRange.baseArrayLayer;
	textureView->layerCount = subresourceRange.layerCount;

	return textureView;
}

Sampler D3D12Renderer::createSampler(SamplerAddressMode addressMode, SamplerFilter minFilter, SamplerFilter magFilter, float anisotropy, svec3 min_max_biasLod, SamplerMipmapMode mipmapMode)
{
	D3D12Sampler *sampler = new D3D12Sampler();

	D3D12_SAMPLER_DESC samplerDesc = {};
	samplerDesc.Filter = samplerFilterToD3D12Filter(minFilter, magFilter, mipmapMode);
	samplerDesc.AddressU = samplerAddressModeToD3D12TextureAddressMode(addressMode);
	samplerDesc.AddressV = samplerAddressModeToD3D12TextureAddressMode(addressMode);
	samplerDesc.AddressW = samplerAddressModeToD3D12TextureAddressMode(addressMode);
	samplerDesc.MipLODBias = min_max_biasLod.z;
	samplerDesc.MaxAnisotropy = UINT(anisotropy);
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = 0.0f;
	samplerDesc.BorderColor[3] = 1.0f;
	samplerDesc.MinLOD = min_max_biasLod.x;
	samplerDesc.MaxLOD = min_max_biasLod.y;

	sampler->samplerDesc = samplerDesc;

	return sampler;
}

Buffer D3D12Renderer::createBuffer(size_t size, BufferUsageFlags usage, BufferLayout initialLayout, MemoryUsage memUsage, bool ownMemory)
{
	D3D12Buffer *buffer = new D3D12Buffer();
	buffer->bufferSize = size;
	buffer->usage = usage;

	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;

	switch (memUsage)
	{
		case MEMORY_USAGE_GPU_ONLY:
			heapType = D3D12_HEAP_TYPE_DEFAULT;
			break;
		case MEMORY_USAGE_CPU_ONLY:
		case MEMORY_USAGE_CPU_TO_GPU:
			heapType = D3D12_HEAP_TYPE_UPLOAD;
			break;
	}

	// TODO Validate layout against usage flags

	D3D12_RESOURCE_STATES initialState = BufferLayoutToD3D12ResourceStates(initialLayout);

	if (heapType == D3D12_HEAP_TYPE_UPLOAD)
	{
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	D3D12_RESOURCE_DESC bufferDesc = {};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Alignment = 0;
	bufferDesc.Width = size;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufferDesc.SampleDesc = {1, 0};
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufferDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	if (usage & BUFFER_USAGE_STORAGE_BUFFER_BIT)
		bufferDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	DX_CHECK_RESULT(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(heapType), D3D12_HEAP_FLAG_NONE, &bufferDesc, initialState, nullptr, IID_PPV_ARGS(&buffer->bufferResource)));

	return buffer;
}

void *D3D12Renderer::mapBuffer(Buffer buffer)
{
	D3D12Buffer *d3dbuffer = static_cast<D3D12Buffer*>(buffer);
	void *memLoc = nullptr;

	CD3DX12_RANGE readRange(0, 0);
	DX_CHECK_RESULT(d3dbuffer->bufferResource->Map(0, &readRange, &memLoc));

	return memLoc;
}

void D3D12Renderer::unmapBuffer(Buffer buffer)
{
	CD3DX12_RANGE readRange(0, 0);
	static_cast<D3D12Buffer*>(buffer)->bufferResource->Unmap(0, &readRange);
}

StagingBuffer D3D12Renderer::createStagingBuffer(size_t size)
{
	D3D12StagingBuffer *stagingBuffer = new D3D12StagingBuffer();
	stagingBuffer->bufferSize = size;

	DX_CHECK_RESULT(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(size), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&stagingBuffer->bufferResource)));

	return stagingBuffer;
}

StagingBuffer D3D12Renderer::createAndFillStagingBuffer(size_t dataSize, const void *data)
{
	D3D12StagingBuffer *stagingBuffer = static_cast<D3D12StagingBuffer*>(createStagingBuffer(dataSize));
	void *memLoc = nullptr;

	CD3DX12_RANGE readRange(0, 0);
	DX_CHECK_RESULT(stagingBuffer->bufferResource->Map(0, &readRange, &memLoc));
	memcpy(memLoc, data, dataSize);
	stagingBuffer->bufferResource->Unmap(0, &readRange);

	return stagingBuffer;
}

void D3D12Renderer::fillStagingBuffer(StagingBuffer stagingBuffer, size_t dataSize, const void *data)
{
	void *memLoc = mapStagingBuffer(stagingBuffer);
	memcpy(memLoc, data, dataSize);
	unmapStagingBuffer(stagingBuffer);
}

void *D3D12Renderer::mapStagingBuffer(StagingBuffer stagingBuffer)
{
	D3D12StagingBuffer *d3dstagingBuffer = static_cast<D3D12StagingBuffer*>(stagingBuffer);
	void *memLoc = nullptr;

	CD3DX12_RANGE readRange(0, 0);
	DX_CHECK_RESULT(d3dstagingBuffer->bufferResource->Map(0, &readRange, &memLoc));

	return memLoc;
}

void D3D12Renderer::unmapStagingBuffer(StagingBuffer stagingBuffer)
{
	D3D12StagingBuffer *d3dstagingBuffer = static_cast<D3D12StagingBuffer*>(stagingBuffer);
	CD3DX12_RANGE readRange(0, 0);

	d3dstagingBuffer->bufferResource->Unmap(0, &readRange);
}

StagingTexture D3D12Renderer::createStagingTexture(suvec3 extent, ResourceFormat format, uint32_t mipLevelCount, uint32_t arrayLayerCount)
{
	D3D12_RESOURCE_DESC textureResourceDesc = {};
	textureResourceDesc.Dimension = extent.z > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE3D : (extent.y > 1 ? D3D12_RESOURCE_DIMENSION_TEXTURE2D : D3D12_RESOURCE_DIMENSION_TEXTURE1D);
	textureResourceDesc.Alignment = 0;
	textureResourceDesc.Width = extent.x;
	textureResourceDesc.Height = extent.y;
	textureResourceDesc.DepthOrArraySize = std::max<uint32_t>(extent.z, arrayLayerCount);
	textureResourceDesc.MipLevels = mipLevelCount;
	textureResourceDesc.Format = ResourceFormatToDXGIFormat(format);
	textureResourceDesc.SampleDesc = {1, 0};
	textureResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	textureResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	uint32_t numSubresources = mipLevelCount * arrayLayerCount;
	uint64_t totalSize = 0;

	D3D12StagingTexture *stagingTexture = new D3D12StagingTexture();
	stagingTexture->placedSubresourceFootprints.resize(numSubresources);
	stagingTexture->subresourceNumRows.resize(numSubresources);
	stagingTexture->subresourceRowSize.resize(numSubresources);

	device->GetCopyableFootprints(&textureResourceDesc, 0, numSubresources, 0, stagingTexture->placedSubresourceFootprints.data(), stagingTexture->subresourceNumRows.data(), stagingTexture->subresourceRowSize.data(), &totalSize);

	stagingTexture->bufferSize = totalSize;
	stagingTexture->textureFormat = format;
	stagingTexture->width = extent.x;
	stagingTexture->height = extent.y;
	stagingTexture->depth = extent.z;
	stagingTexture->mipLevels = mipLevelCount;
	stagingTexture->arrayLayers = arrayLayerCount;

	DX_CHECK_RESULT(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE, &CD3DX12_RESOURCE_DESC::Buffer(totalSize), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&stagingTexture->bufferResource)));

	return stagingTexture;
}

void D3D12Renderer::fillStagingTextureSubresource(StagingTexture stagingTexture, const void *textureData, uint32_t mipLevel, uint32_t arrayLayer)
{
	D3D12StagingTexture *d3dstagingTexture = static_cast<D3D12StagingTexture*>(stagingTexture);

	uint32_t subresourceIndex = arrayLayer * d3dstagingTexture->mipLevels + arrayLayer;
	uint32_t subresourceRows = d3dstagingTexture->subresourceNumRows[subresourceIndex];
	uint64_t subresourceRowSize = d3dstagingTexture->subresourceRowSize[subresourceIndex];

	const D3D12_PLACED_SUBRESOURCE_FOOTPRINT &placedSubresourceFootprint = d3dstagingTexture->placedSubresourceFootprints[subresourceIndex];

	uint32_t subresourceSlicePitch = placedSubresourceFootprint.Footprint.RowPitch * placedSubresourceFootprint.Footprint.Height;

	D3D12_RANGE mapReadRange = {0, 0};

	char *mappedMem = nullptr;
	DX_CHECK_RESULT(d3dstagingTexture->bufferResource->Map(0, &mapReadRange, reinterpret_cast<void**>(&mappedMem)));

	mappedMem += placedSubresourceFootprint.Offset;

	for (uint32_t z = 0; z < placedSubresourceFootprint.Footprint.Depth; z++)
	{
		char *dstSlice = mappedMem + subresourceSlicePitch * z;
		const char *srcSlice = reinterpret_cast<const char*>(textureData) + subresourceRowSize * placedSubresourceFootprint.Footprint.Height * z;

		for (uint32_t y = 0; y < subresourceRows; y++)
		{
			memcpy(dstSlice + placedSubresourceFootprint.Footprint.RowPitch * y, srcSlice + subresourceRowSize * y, subresourceRowSize);
		}
	}

	d3dstagingTexture->bufferResource->Unmap(0, nullptr);
}

void D3D12Renderer::destroyCommandPool(CommandPool pool)
{
	delete static_cast<D3D12CommandPool*>(pool);
}

void D3D12Renderer::destroyRenderGraph(RenderGraph &graph)
{
	delete graph;
	graph = nullptr;
}

void D3D12Renderer::destroyPipeline(Pipeline pipeline)
{
	D3D12Pipeline *d3d12Pipeline = static_cast<D3D12Pipeline*>(pipeline);
	d3d12Pipeline->pipeline->Release();
	d3d12Pipeline->rootSignature->Release();

	delete d3d12Pipeline;
}

void D3D12Renderer::destroyShaderModule(ShaderModule shaderModule)
{
	delete static_cast<D3D12ShaderModule*>(shaderModule);
}

void D3D12Renderer::destroyDescriptorPool(DescriptorPool pool)
{
	D3D12DescriptorPool *d3d12Pool = static_cast<D3D12DescriptorPool*>(pool);

	delete d3d12Pool;
}

void D3D12Renderer::destroyTexture(Texture texture)
{
	D3D12Texture *d3d12Texture = static_cast<D3D12Texture*>(texture);

	d3d12Texture->textureResource->Release();

	delete d3d12Texture;
}

void D3D12Renderer::destroyTextureView(TextureView textureView)
{
	D3D12TextureView *d3d12TV = static_cast<D3D12TextureView*>(textureView);

	delete d3d12TV;
}

void D3D12Renderer::destroySampler(Sampler sampler)
{
	D3D12Sampler *d3d12Sampler = static_cast<D3D12Sampler*>(sampler);

	delete d3d12Sampler;
}

void D3D12Renderer::destroyBuffer(Buffer buffer)
{
	D3D12Buffer *d3d12Buffer = static_cast<D3D12Buffer*>(buffer);

	d3d12Buffer->bufferResource->Release();

	delete d3d12Buffer;
}

void D3D12Renderer::destroyStagingBuffer(StagingBuffer stagingBuffer)
{
	D3D12StagingBuffer *d3d12StagingBuffer = static_cast<D3D12StagingBuffer*>(stagingBuffer);

	d3d12StagingBuffer->bufferResource->Release();

	delete d3d12StagingBuffer;
}

void D3D12Renderer::destroyFence(Fence fence)
{
	D3D12Fence *d3d12Fence = static_cast<D3D12Fence*>(fence);

	d3d12Fence->fence->Release();
	CloseHandle(d3d12Fence->fenceEvent);

	delete d3d12Fence;
}

void D3D12Renderer::destroySemaphore(Semaphore sem)
{
	D3D12Semaphore *d3d12Sem = static_cast<D3D12Semaphore*>(sem);

	d3d12Sem->semFence->Release();
	CloseHandle(d3d12Sem->semFenceWaitEvent);

	delete d3d12Sem;
}

void D3D12Renderer::destroyStagingTexture(StagingTexture stagingTexture)
{
	D3D12StagingTexture *d3d12StagingTexture = static_cast<D3D12StagingTexture*>(stagingTexture);

	d3d12StagingTexture->bufferResource->Release();

	delete d3d12StagingTexture;
}

void D3D12Renderer::setObjectDebugName(void *obj, RendererObjectType objType, const std::string & name)
{
	switch (objType)
	{
		case OBJECT_TYPE_SEMAPHORE:
			static_cast<D3D12Semaphore*>(obj)->semFence->SetName(utf8_to_utf16(name).c_str());
			break;
		case OBJECT_TYPE_COMMAND_BUFFER:
			static_cast<D3D12CommandBuffer*>(obj)->cmdList->SetName(utf8_to_utf16(name).c_str());
			break;
		case OBJECT_TYPE_FENCE:
			static_cast<D3D12Fence*>(obj)->fence->SetName(utf8_to_utf16(name).c_str());
			break;
		case OBJECT_TYPE_BUFFER:
			static_cast<D3D12Buffer*>(obj)->bufferResource->SetName(utf8_to_utf16(name).c_str());
			break;
		case OBJECT_TYPE_TEXTURE:
			static_cast<D3D12Texture*>(obj)->textureResource->SetName(utf8_to_utf16(name).c_str());
			break;
		case OBJECT_TYPE_TEXTURE_VIEW:
			break;
		case OBJECT_TYPE_SHADER_MODULE:
			break;
		case OBJECT_TYPE_PIPELINE:
			static_cast<D3D12Pipeline*>(obj)->pipeline->SetName(utf8_to_utf16(name).c_str());
			break;
		case OBJECT_TYPE_SAMPLER:
			break;
		case OBJECT_TYPE_DESCRIPTOR_SET:
			break;
		case OBJECT_TYPE_COMMAND_POOL:
			static_cast<D3D12CommandPool*>(obj)->cmdAlloc->SetName(utf8_to_utf16(name).c_str());
			static_cast<D3D12CommandPool*>(obj)->cmdAlloc->SetName(utf8_to_utf16(name + "_bundle").c_str());
			break;
	}
}

void D3D12Renderer::initSwapchain(Window *wnd)
{
	swapchainHandler->initSwapchain(wnd);
}

void D3D12Renderer::presentToSwapchain(Window *wnd, std::vector<Semaphore> waitSemaphores)
{
	swapchainHandler->presentToSwapchain(wnd);
}

void D3D12Renderer::recreateSwapchain(Window *wnd)
{
	swapchainHandler->recreateSwapchain(wnd);
}

void D3D12Renderer::setSwapchainTexture(Window *wnd, TextureView texView, Sampler sampler, TextureLayout layout)
{
	D3D12TextureView *d3dtv = static_cast<D3D12TextureView*>(texView);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = ResourceFormatToDXGIFormat(d3dtv->viewFormat);
	srvDesc.ViewDimension = textureViewTypeToD3D12SRVDimension(d3dtv->viewType);
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;

	Log::get()->info("{}", srvDesc.ViewDimension);

	swapchainHandler->setSwapchainSourceTexture(srvDesc, static_cast<D3D12Texture*>(d3dtv->parentTexture)->textureResource);
}

#endif