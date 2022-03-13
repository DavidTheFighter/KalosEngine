#include "RendererCore/D3D12/D3D12RenderGraph.h"
#if BUILD_D3D12_BACKEND

#include <RendererCore/Renderer.h>
#include <RendererCore/D3D12/D3D12Renderer.h>

#include <RendererCore/RendererObjects.h>

#include <RendererCore/D3D12/D3D12Objects.h>
#include <RendererCore/D3D12/D3D12Enums.h>

D3D12RenderGraph::D3D12RenderGraph(D3D12Renderer *rendererPtr)
{
	renderer = rendererPtr;

	execCounter = 0;

	enableRenderPassMerging = false;

	for (size_t i = 0; i < renderer->getMaxFramesInFlight(); i ++)
		gfxCommandPools.push_back(renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT | COMMAND_POOL_RESET_COMMAND_BUFFER_BIT));

	for (size_t i = 0; i < renderer->getMaxFramesInFlight(); i++)
		executionDoneSemaphores.push_back(renderer->createSemaphore());

	for (size_t i = 0; i < renderer->getMaxFramesInFlight(); i++)
		gfxCommandBuffers.push_back(gfxCommandPools[i]->allocateCommandBuffer());

	rtvDescriptorSize = renderer->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

D3D12RenderGraph::~D3D12RenderGraph()
{
	cleanupResources();
}

Semaphore D3D12RenderGraph::execute(bool returnWaitableSemaphore)
{
	D3D12CommandBuffer *cmdBuffer = static_cast<D3D12CommandBuffer*>(gfxCommandBuffers[execCounter % gfxCommandBuffers.size()]);
	gfxCommandPools[execCounter % gfxCommandPools.size()]->resetCommandPool();

	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	for (size_t i = 0; i < finalPasses.size(); i++)
	{
		const D3D12RenderGraphRenderPassData &passData = finalPasses[i];
		RenderGraphRenderPass &pass = *passes[passData.passIndex];

		if (passData.beforeRenderBarriers.size() > 0)
			cmdBuffer->d3d12_resourceBarrier(passData.beforeRenderBarriers);

		if (pass.getPipelineType() == RENDER_GRAPH_PIPELINE_TYPE_GRAPHICS)
		{
			for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
				if (pass.getColorAttachmentOutputs()[o].clearAttachment)
					cmdBuffer->d3d12_clearRenderTargetView({graphRTVs[passData.passIndex]->GetCPUDescriptorHandleForHeapStart().ptr + rtvDescriptorSize * o}, pass.getColorAttachmentOutputs()[o].attachmentClearValue);

			if (pass.hasDepthStencilOutput())
				if (pass.getDepthStencilAttachmentOutput().clearAttachment)
					cmdBuffer->d3d12_clearDepthStencilView(graphDSVs[passData.passIndex]->GetCPUDescriptorHandleForHeapStart(), pass.getDepthStencilAttachmentOutput().attachmentClearValue.depthStencil.depth, pass.getDepthStencilAttachmentOutput().attachmentClearValue.depthStencil.stencil);

			if (pass.getColorAttachmentOutputs().size() > 0)
				cmdBuffer->d3d12_OMSetRenderTargets({graphRTVs[passData.passIndex]->GetCPUDescriptorHandleForHeapStart()}, true, graphDSVs[passData.passIndex] != nullptr ? &graphDSVs[passData.passIndex]->GetCPUDescriptorHandleForHeapStart() : nullptr);
			else
				cmdBuffer->d3d12_OMSetRenderTargets({}, false, &graphDSVs[passData.passIndex]->GetCPUDescriptorHandleForHeapStart());

			cmdBuffer->setViewports(0, {{0, 0, (float) passData.sizeX, (float) passData.sizeY, 0, 1}});
			cmdBuffer->setScissors(0, {{0, 0, passData.sizeX, passData.sizeY}});
		}

		RenderGraphRenderFunctionData renderData = {};

		if (pass.hasRenderFunction())
			pass.getRenderFunction()(cmdBuffer, renderData);

		if (passData.afterRenderBarriers.size() > 0)
			cmdBuffer->d3d12_resourceBarrier(passData.afterRenderBarriers);
	}

	cmdBuffer->endCommands();

	std::vector<Semaphore> waitSems = {};
	std::vector<PipelineStageFlags> waitSemStages = {};
	std::vector<Semaphore> signalSems = {};

	if (returnWaitableSemaphore)
		signalSems.push_back(executionDoneSemaphores[execCounter % executionDoneSemaphores.size()]);

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, waitSems, waitSemStages, {}, nullptr);

	execCounter++;

	return returnWaitableSemaphore ? signalSems[0] : nullptr;
}

void D3D12RenderGraph::resizeNamedSize(const std::string &sizeName, glm::uvec2 newSize)
{
	namedSizes[sizeName] = newSize;

	std::vector<D3D12Texture*> resizedTextures;

	for (D3D12RenderGraphTexture &texture : graphTextures)
	{
		if (texture.attachment.namedRelativeSize == sizeName)
		{
			resizedTextures.push_back(texture.rendererTexture);
			texture.rendererTexture->textureResource->Release();

			D3D12_RESOURCE_DESC texDesc = {};
			texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			texDesc.Alignment = 0;
			texDesc.Width = newSize.x;
			texDesc.Height = newSize.y;
			texDesc.DepthOrArraySize = texture.attachment.arrayLayers;
			texDesc.MipLevels = texture.attachment.mipLevels;
			texDesc.Format = ResourceFormatToDXGIFormat(texture.attachment.format);
			texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
			texDesc.Flags = texture.rendererTexture->usage;

			switch (texture.attachment.samples)
			{
				case 1:
					texDesc.SampleDesc = {1, 0};
					break;
				case 2:
					texDesc.SampleDesc = {2, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
					break;
				case 4:
					texDesc.SampleDesc = {4, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
					break;
				case 8:
					texDesc.SampleDesc = {8, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
					break;
				case 16:
					texDesc.SampleDesc = {16, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
					break;
				default:
					texDesc.SampleDesc = {1, 0};
			}

			D3D12_RESOURCE_STATES textureState = graphResourcesInitialResourceState[texture.name];

			bool useClearValue = (texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

			DX_CHECK_RESULT(renderer->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc, textureState, useClearValue ? &texture.rendererTexture->clearValue : nullptr, IID_PPV_ARGS(&texture.rendererTexture->textureResource)));
		}
	}

	for (D3D12RenderGraphRenderPassData &passData : finalPasses)
	{
		RenderGraphRenderPass &pass = *passes[passData.passIndex].get();

		ID3D12DescriptorHeap *rtvHeap = graphRTVs[passData.passIndex], *dsvHeap = graphDSVs[passData.passIndex];

		if (pass.getColorAttachmentOutputs().size() > 0)
		{
			D3D12_DESCRIPTOR_HEAP_DESC passRTVDesc = {};
			passRTVDesc.NumDescriptors = pass.getColorAttachmentOutputs().size();
			passRTVDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

			for (size_t o = 0; o < passRTVDesc.NumDescriptors; o++)
				renderer->device->CreateRenderTargetView(static_cast<D3D12Texture *>(graphTextureViews[pass.getColorAttachmentOutputs()[o].textureName]->parentTexture)->textureResource, nullptr, {rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + rtvDescriptorSize * o});
		}

		if (pass.hasDepthStencilOutput())
		{
			D3D12_DESCRIPTOR_HEAP_DESC passDSVDesc = {};
			passDSVDesc.NumDescriptors = 1;
			passDSVDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			passDSVDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

			renderer->device->CreateDepthStencilView(static_cast<D3D12Texture *>(graphTextureViews[pass.getDepthStencilAttachmentOutput().textureName]->parentTexture)->textureResource, nullptr, dsvHeap->GetCPUDescriptorHandleForHeapStart());
		}
	}

	buildBarrierLists(finalPassStack);
}

void D3D12RenderGraph::cleanupResources()
{
	for (auto it = graphTextureViews.begin(); it != graphTextureViews.end(); it++)
		renderer->destroyTextureView(it->second);

	for (auto it = graphBuffers.begin(); it != graphBuffers.end(); it++)
		renderer->destroyBuffer(it->second);

	for (size_t i = 0; i < graphTextures.size(); i++)
	{
		graphTextures[i].rendererTexture->textureResource->Release();
		delete graphTextures[i].rendererTexture;
	}

	for (auto it = graphRTVs.begin(); it != graphRTVs.end(); it++)
		if (it->second != nullptr)
			it->second->Release();

	for (auto it = graphDSVs.begin(); it != graphDSVs.end(); it++)
		if (it->second != nullptr)
			it->second->Release();

	for (CommandPool pool : gfxCommandPools)
		renderer->destroyCommandPool(pool);

	for (Semaphore sem : executionDoneSemaphores)
		renderer->destroySemaphore(sem);

	executionDoneSemaphores.clear();
	gfxCommandPools.clear();
	gfxCommandBuffers.clear();
	graphTextures.clear();
	graphTextureViews.clear();
	graphRTVs.clear();
	graphDSVs.clear();

	attachmentUsageLifetimes.clear();
}

TextureView D3D12RenderGraph::getRenderGraphOutputTextureView()
{
	return graphTextureViews[outputAttachmentName];
}

const std::string viewSelectMipPostfix = "_fg_miplvl";
const std::string viewSelectLayerPostfix = "_fg_layer";

void D3D12RenderGraph::assignPhysicalResources(const std::vector<size_t>& passStack)
{
	typedef struct
	{
		RenderPassAttachment attachment;
		D3D12_RESOURCE_FLAGS usageFlags;
		ClearValue clearValue;

	} PhysicalTextureData;

	typedef struct
	{
		uint32_t size;
		D3D12_RESOURCE_FLAGS usageFlags;

	} PhysicalBufferData;

	std::map<std::string, PhysicalTextureData> textureData;
	std::map<std::string, PhysicalBufferData> bufferData;

	// Gather information about each resource and how it will be used
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RenderGraphRenderPass &pass = *passes[passStack[i]];

		for (size_t i = 0; i < pass.getSampledTextureInputs().size(); i++)
			graphResourcesInitialResourceState[pass.getSampledTextureInputs()[i]] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(textureData[pass.getSampledTextureInputs()[i]].attachment.format) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);

		for (size_t i = 0; i < pass.getInputAttachmentInputs().size(); i++)
			graphResourcesInitialResourceState[pass.getInputAttachmentInputs()[i]] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(textureData[pass.getSampledTextureInputs()[i]].attachment.format) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);

		for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
		{
			PhysicalTextureData data = {};
			data.attachment = pass.getColorAttachmentOutputs()[o].attachment;
			data.usageFlags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			data.clearValue = pass.getColorAttachmentOutputs()[o].attachmentClearValue;

			const std::string &textureName = pass.getColorAttachmentOutputs()[o].textureName;

			textureData[textureName] = data;
			graphResourcesInitialResourceState[textureName] = outputAttachmentName == textureName ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		if (pass.hasDepthStencilOutput())
		{
			PhysicalTextureData data = {};
			data.attachment = pass.getDepthStencilAttachmentOutput().attachment;
			data.usageFlags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			data.clearValue = pass.getDepthStencilAttachmentOutput().attachmentClearValue;
			
			const std::string &textureName = pass.getDepthStencilAttachmentOutput().textureName;

			textureData[textureName] = data;
			graphResourcesInitialResourceState[textureName] = outputAttachmentName == textureName ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		for (size_t o = 0; o < pass.getStorageTextures().size(); o++)
		{
			const std::string &textureName = pass.getStorageTextures()[o].textureName;

			if (pass.getStorageTextures()[o].canWriteAsOutput)
			{
				PhysicalTextureData data = {};
				data.attachment = pass.getStorageTextures()[o].attachment;
				data.usageFlags = D3D12_RESOURCE_FLAG_NONE;
				data.clearValue = {0.0f, 0.0f, 0.0f, 0.0f};

				if (data.attachment.samples <= 1)
					data.usageFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				textureData[textureName] = data;
				graphResourcesInitialResourceState[textureName] = outputAttachmentName == textureName ? (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(data.attachment.format) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON)) : TextureLayoutToD3D12ResourceStates(pass.getStorageTextures()[o].passEndLayout);
			}
			else
			{
				PhysicalTextureData &data = textureData[textureName];
				if (data.attachment.samples <= 1)
					data.usageFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				graphResourcesInitialResourceState[textureName] = TextureLayoutToD3D12ResourceStates(pass.getStorageTextures()[o].passEndLayout);
			}
		}

		for (size_t sb = 0; sb < pass.getStorageBuffers().size(); sb++)
		{
			const RenderPassStorageBuffer &storageBuffer = pass.getStorageBuffers()[sb];

			if (storageBuffer.canWriteAsOutput)
			{
				PhysicalBufferData data = {};
				data.size = storageBuffer.size;
				data.usageFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

				bufferData[storageBuffer.bufferName] = data;
				graphResourcesInitialResourceState[storageBuffer.bufferName] = BufferLayoutToD3D12ResourceStates(storageBuffer.passEndLayout);
			}
			else
			{
				graphResourcesInitialResourceState[storageBuffer.bufferName] = BufferLayoutToD3D12ResourceStates(storageBuffer.passEndLayout);
			}
		}
	}

	// Actually create all the resources
	for (auto it = textureData.begin(); it != textureData.end(); it++)
	{
		const PhysicalTextureData &data = it->second;

		uint32_t sizeX = data.attachment.namedRelativeSize == "" ? uint32_t(data.attachment.sizeX) : uint32_t(namedSizes[data.attachment.namedRelativeSize].x * data.attachment.sizeX);
		uint32_t sizeY = data.attachment.namedRelativeSize == "" ? uint32_t(data.attachment.sizeY) : uint32_t(namedSizes[data.attachment.namedRelativeSize].y * data.attachment.sizeY);

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = sizeX;
		texDesc.Height = sizeY;
		texDesc.DepthOrArraySize = data.attachment.arrayLayers;
		texDesc.MipLevels = data.attachment.mipLevels;
		texDesc.Format = ResourceFormatToDXGIFormat(data.attachment.format);
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = data.usageFlags;

		switch (data.attachment.samples)
		{
			case 1:
				texDesc.SampleDesc = {1, 0};
				break;
			case 2:
				texDesc.SampleDesc = {2, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
				break;
			case 4:
				texDesc.SampleDesc = {4, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
				break;
			case 8:
				texDesc.SampleDesc = {8, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
				break;
			case 16:
				texDesc.SampleDesc = {16, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
				break;
			default:
				texDesc.SampleDesc = {1, 0};
		}

		D3D12RenderGraphTexture graphTexture = {};
		graphTexture.attachment = it->second.attachment;
		graphTexture.rendererTexture = new D3D12Texture();
		graphTexture.rendererTexture->width = sizeX;
		graphTexture.rendererTexture->height = sizeY;
		graphTexture.rendererTexture->depth = 1;
		graphTexture.rendererTexture->textureFormat = data.attachment.format;
		graphTexture.rendererTexture->layerCount = data.attachment.arrayLayers;
		graphTexture.rendererTexture->mipCount = data.attachment.mipLevels;
		graphTexture.rendererTexture->usage = texDesc.Flags;
		graphTexture.name = it->first;

		D3D12_RESOURCE_STATES textureState = graphResourcesInitialResourceState[it->first];

		bool useClearValue = (texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		D3D12_CLEAR_VALUE optClearValue = {};
		optClearValue.Format = texDesc.Format;
		memcpy(optClearValue.Color, data.clearValue.color.float32, sizeof(optClearValue.Color));
		graphTexture.rendererTexture->clearValue = optClearValue;

		DX_CHECK_RESULT(renderer->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc, textureState, useClearValue ? &optClearValue : nullptr, IID_PPV_ARGS(&graphTexture.rendererTexture->textureResource)));

		renderer->setObjectDebugName(graphTexture.rendererTexture, OBJECT_TYPE_TEXTURE, it->first);

		graphTextures.push_back(graphTexture);

		D3D12TextureView *graphTextureView = new D3D12TextureView();
		graphTextureView->parentTexture = graphTexture.rendererTexture;
		graphTextureView->viewType = data.attachment.viewType;
		graphTextureView->viewFormat = data.attachment.format;
		graphTextureView->baseMip = 0;
		graphTextureView->mipCount = data.attachment.mipLevels;
		graphTextureView->baseLayer = 0;
		graphTextureView->layerCount = data.attachment.arrayLayers;

		graphTextureViews[it->first] = graphTextureView;

		for (uint32_t m = 0; m < data.attachment.mipLevels; m++)
		{
			graphTextureView = new D3D12TextureView();
			graphTextureView->parentTexture = graphTexture.rendererTexture;
			graphTextureView->viewType = data.attachment.viewType;
			graphTextureView->viewFormat = data.attachment.format;
			graphTextureView->baseMip = m;
			graphTextureView->mipCount = 1;
			graphTextureView->baseLayer = 0;
			graphTextureView->layerCount = data.attachment.arrayLayers;

			graphTextureViews[it->first + viewSelectMipPostfix + toString(m)] = graphTextureView;
		}

		for (uint32_t l = 0; l < data.attachment.arrayLayers; l++)
		{
			graphTextureView = new D3D12TextureView();
			graphTextureView->parentTexture = graphTexture.rendererTexture;
			graphTextureView->viewType = data.attachment.viewType;
			graphTextureView->viewFormat = data.attachment.format;
			graphTextureView->baseMip = 0;
			graphTextureView->mipCount = 1;
			graphTextureView->baseLayer = l;
			graphTextureView->layerCount = 1;

			graphTextureViews[it->first + viewSelectLayerPostfix + toString(l)] = graphTextureView;
		}
	}

	for (auto bufferIt = bufferData.begin(); bufferIt != bufferData.end(); bufferIt++)
	{
		const PhysicalBufferData &data = bufferIt->second;

		D3D12_RESOURCE_DESC bufferDesc = {};
		bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		bufferDesc.Alignment = 0;
		bufferDesc.Width = data.size;
		bufferDesc.Height = 1;
		bufferDesc.DepthOrArraySize = 1;
		bufferDesc.MipLevels = 1;
		bufferDesc.Format = DXGI_FORMAT_UNKNOWN;
		bufferDesc.SampleDesc = {1, 0};
		bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		bufferDesc.Flags = data.usageFlags;
		
		D3D12Buffer *buffer = new D3D12Buffer();
		buffer->bufferSize = data.size;

		DX_CHECK_RESULT(renderer->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &bufferDesc, graphResourcesInitialResourceState[bufferIt->first], nullptr, IID_PPV_ARGS(&buffer->bufferResource)));

		graphBuffers[bufferIt->first] = buffer;
	}
}

void D3D12RenderGraph::finishBuild(const std::vector<size_t>& passStack)
{
	// Create RTVs and DSVs, also initialize the passes
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RenderGraphRenderPass &pass = *passes[passStack[i]].get();

		ID3D12DescriptorHeap *rtvHeap = nullptr, *dsvHeap = nullptr;

		if (pass.getColorAttachmentOutputs().size() > 0)
		{
			D3D12_DESCRIPTOR_HEAP_DESC passRTVDesc = {};
			passRTVDesc.NumDescriptors = pass.getColorAttachmentOutputs().size();
			passRTVDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

			DX_CHECK_RESULT(renderer->device->CreateDescriptorHeap(&passRTVDesc, IID_PPV_ARGS(&rtvHeap)));

			for (size_t o = 0; o < passRTVDesc.NumDescriptors; o++)
				renderer->device->CreateRenderTargetView(static_cast<D3D12Texture*>(graphTextureViews[pass.getColorAttachmentOutputs()[o].textureName]->parentTexture)->textureResource, nullptr, {rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + rtvDescriptorSize * o});
		}

		if (pass.hasDepthStencilOutput())
		{
			D3D12_DESCRIPTOR_HEAP_DESC passDSVDesc = {};
			passDSVDesc.NumDescriptors = 1;
			passDSVDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			passDSVDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			
			DX_CHECK_RESULT(renderer->device->CreateDescriptorHeap(&passDSVDesc, IID_PPV_ARGS(&dsvHeap)));

			renderer->device->CreateDepthStencilView(static_cast<D3D12Texture*>(graphTextureViews[pass.getDepthStencilAttachmentOutput().textureName]->parentTexture)->textureResource, nullptr, dsvHeap->GetCPUDescriptorHandleForHeapStart());
		}

		graphRTVs[passStack[i]] = rtvHeap;
		graphDSVs[passStack[i]] = dsvHeap;

		std::vector<AttachmentDescription> tempAttDesc;
		std::vector<AttachmentReference> tempAttRefs;

		for (size_t a = 0; a < pass.getColorAttachmentOutputs().size(); a++)
		{
			AttachmentDescription desc = {};
			desc.format = pass.getColorAttachmentOutputs()[a].attachment.format;

			tempAttDesc.push_back(desc);

			AttachmentReference ref = {};
			ref.attachment = a;

			tempAttRefs.push_back(ref);
		}

		if (pass.hasDepthStencilOutput())
		{
			AttachmentDescription desc = {};
			desc.format = pass.getDepthStencilAttachmentOutput().attachment.format;

			tempAttDesc.push_back(desc);
		}

		SubpassDescription tempSubpassDesc = {};
		tempSubpassDesc.colorAttachments = tempAttRefs;
		tempSubpassDesc.hasDepthAttachment = pass.hasDepthStencilOutput();

		if (pass.hasDepthStencilOutput())
		{
			AttachmentReference ref = {};
			ref.attachment = tempAttDesc.size() - 1;

			tempSubpassDesc.depthStencilAttachment = ref;
		}

		std::unique_ptr<RendererRenderPass> tempRenderPass(new RendererRenderPass());
		tempRenderPass->attachments = tempAttDesc;
		tempRenderPass->subpasses.push_back(tempSubpassDesc);

		RenderGraphInitFunctionData initData = {};
		initData.baseSubpass = 0;
		initData.renderPassHandle = tempRenderPass.get();

		if (pass.hasInitFunction())
			pass.getInitFunction()(initData);
	}

	finalPassStack = passStack;

	buildBarrierLists(passStack);

	RenderGraphDescriptorUpdateFunctionData descUpdateData = {};
	descUpdateData.graphTextureViews = graphTextureViews;
	descUpdateData.graphBuffers = graphBuffers;

	for (size_t i = 0; i < finalPasses.size(); i++)
		if (passes[finalPasses[i].passIndex]->hasDescriptorUpdateFunction())
			passes[finalPasses[i].passIndex]->getDescriptorUpdateFunction()(descUpdateData);
}

void D3D12RenderGraph::buildBarrierLists(const std::vector<size_t> &passStack)
{
	if (finalPasses.size() > 0)
		finalPasses.clear();

	typedef struct
	{
		D3D12_RESOURCE_STATES currentState;

	} RenderPassResourceState;

	std::map<std::string, RenderPassResourceState> resourceStates;

	/*
	Initialize all resources with the last state they would be in after a full render graph execution,
	as this will be the first state we transition out of when we execute it again
	*/
	for (auto initialResourceStatesIt = graphResourcesInitialResourceState.begin(); initialResourceStatesIt != graphResourcesInitialResourceState.end(); initialResourceStatesIt++)
	{
		RenderPassResourceState state = {};
		state.currentState = initialResourceStatesIt->second;

		resourceStates[initialResourceStatesIt->first] = state;
	}

	for (size_t p = 0; p < passStack.size(); p++)
	{
		const RenderGraphRenderPass &pass = *passes[passStack[p]];
		D3D12RenderGraphRenderPassData passData = {};

		passData.passIndex = passStack[p];

		if (pass.hasDepthStencilOutput())
		{
			const RenderPassAttachment &attachment = pass.getDepthStencilAttachmentOutput().attachment;

			passData.sizeX = attachment.namedRelativeSize == "" ? uint32_t(attachment.sizeX) : uint32_t(namedSizes[attachment.namedRelativeSize].x * attachment.sizeX);
			passData.sizeY = attachment.namedRelativeSize == "" ? uint32_t(attachment.sizeY) : uint32_t(namedSizes[attachment.namedRelativeSize].y * attachment.sizeY);
		}
		else if (pass.getColorAttachmentOutputs().size() > 0)
		{
			const RenderPassAttachment &attachment = pass.getColorAttachmentOutputs()[0].attachment;

			passData.sizeX = attachment.namedRelativeSize == "" ? uint32_t(attachment.sizeX) : uint32_t(namedSizes[attachment.namedRelativeSize].x * attachment.sizeX);
			passData.sizeY = attachment.namedRelativeSize == "" ? uint32_t(attachment.sizeY) : uint32_t(namedSizes[attachment.namedRelativeSize].y * attachment.sizeY);
		}

		for (size_t i = 0; i < pass.getSampledTextureInputs().size(); i++)
		{
			const std::string &inputName = pass.getSampledTextureInputs()[i];
			D3D12_RESOURCE_STATES inputCurrentState = resourceStates[inputName].currentState;
			D3D12_RESOURCE_STATES inputRequiredState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(graphTextureViews[inputName]->viewFormat) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);

			if (inputCurrentState != inputRequiredState)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture *>(graphTextureViews[inputName]->parentTexture)->textureResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = inputCurrentState;
				transitionBarrier.StateAfter = inputRequiredState;

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.beforeRenderBarriers.push_back(barrier);
			}

			resourceStates[inputName].currentState = inputRequiredState;
		}

		for (size_t i = 0; i < pass.getInputAttachmentInputs().size(); i++)
		{
			const std::string &inputName = pass.getInputAttachmentInputs()[i];
			D3D12_RESOURCE_STATES inputCurrentState = resourceStates[inputName].currentState;
			D3D12_RESOURCE_STATES inputRequiredState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(graphTextureViews[inputName]->viewFormat) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);

			if (inputCurrentState != inputRequiredState)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture *>(graphTextureViews[inputName]->parentTexture)->textureResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = inputCurrentState;
				transitionBarrier.StateAfter = inputRequiredState;

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.beforeRenderBarriers.push_back(barrier);
			}

			resourceStates[inputName].currentState = inputRequiredState;
		}

		for (size_t st = 0; st < pass.getStorageTextures().size(); st++)
		{
			const std::string &storageTextureName = pass.getStorageTextures()[st].textureName;
			D3D12_RESOURCE_STATES inputCurrentState = resourceStates[storageTextureName].currentState;
			D3D12_RESOURCE_STATES inputRequiredState = TextureLayoutToD3D12ResourceStates(pass.getStorageTextures()[st].passBeginLayout);

			if (inputCurrentState != inputRequiredState)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture *>(graphTextureViews[storageTextureName]->parentTexture)->textureResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = inputCurrentState;
				transitionBarrier.StateAfter = inputRequiredState;

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.beforeRenderBarriers.push_back(barrier);
			}

			resourceStates[storageTextureName].currentState = TextureLayoutToD3D12ResourceStates(pass.getStorageTextures()[st].passEndLayout);

			if (outputAttachmentName == storageTextureName)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture *>(graphTextureViews[storageTextureName]->parentTexture)->textureResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = TextureLayoutToD3D12ResourceStates(pass.getStorageTextures()[st].passEndLayout);
				transitionBarrier.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(graphTextureViews[storageTextureName]->viewFormat) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.afterRenderBarriers.push_back(barrier);

				resourceStates[storageTextureName].currentState = transitionBarrier.StateAfter;
			}
		}

		for (size_t sb = 0; sb < pass.getStorageBuffers().size(); sb++)
		{
			const RenderPassStorageBuffer &storageBuffer = pass.getStorageBuffers()[sb];
			D3D12_RESOURCE_STATES currentState = resourceStates[storageBuffer.bufferName].currentState;
			D3D12_RESOURCE_STATES requiredState = BufferLayoutToD3D12ResourceStates(storageBuffer.passBeginLayout);

			if (currentState != requiredState)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Buffer *>(graphBuffers[storageBuffer.bufferName])->bufferResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = currentState;
				transitionBarrier.StateAfter = requiredState;

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.beforeRenderBarriers.push_back(barrier);
			}

			resourceStates[storageBuffer.bufferName].currentState = BufferLayoutToD3D12ResourceStates(storageBuffer.passEndLayout);
		}

		for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
		{
			const std::string &outputName = pass.getColorAttachmentOutputs()[o].textureName;
			D3D12_RESOURCE_STATES inputCurrentState = resourceStates[outputName].currentState;
			D3D12_RESOURCE_STATES inputRequiredState = D3D12_RESOURCE_STATE_RENDER_TARGET;

			if (inputCurrentState != inputRequiredState)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture *>(graphTextureViews[outputName]->parentTexture)->textureResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = inputCurrentState;
				transitionBarrier.StateAfter = inputRequiredState;

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.beforeRenderBarriers.push_back(barrier);
			}

			resourceStates[outputName].currentState = inputRequiredState;

			if (outputAttachmentName == outputName)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture *>(graphTextureViews[outputName]->parentTexture)->textureResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = inputRequiredState;
				transitionBarrier.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.afterRenderBarriers.push_back(barrier);

				resourceStates[outputName].currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			}
		}

		if (pass.hasDepthStencilOutput())
		{
			const std::string &outputName = pass.getDepthStencilAttachmentOutput().textureName;
			D3D12_RESOURCE_STATES inputCurrentState = resourceStates[outputName].currentState;
			D3D12_RESOURCE_STATES inputRequiredState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

			if (inputCurrentState != inputRequiredState)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture *>(graphTextureViews[outputName]->parentTexture)->textureResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = inputCurrentState;
				transitionBarrier.StateAfter = inputRequiredState;

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.beforeRenderBarriers.push_back(barrier);
			}

			resourceStates[outputName].currentState = inputRequiredState;

			if (outputAttachmentName == outputName)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture *>(graphTextureViews[outputName]->parentTexture)->textureResource;
				transitionBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				transitionBarrier.StateBefore = inputRequiredState;
				transitionBarrier.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;

				D3D12_RESOURCE_BARRIER barrier = {};
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Transition = transitionBarrier;

				passData.afterRenderBarriers.push_back(barrier);

				resourceStates[outputName].currentState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;
			}
		}

		finalPasses.push_back(passData);
	}
}

#endif