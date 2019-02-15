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

	for (size_t i = 0; i < 3; i ++)
		gfxCommandPools.push_back(renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT | COMMAND_POOL_RESET_COMMAND_BUFFER_BIT));

	for (size_t i = 0; i < 3; i++)
		executionDoneSemaphores.push_back(renderer->createSemaphore());

	for (size_t i = 0; i < 3; i++)
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

		pass.getRenderFunction()(cmdBuffer, renderData);

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
	renderer->waitForDeviceIdle();

	cleanupResources();

	namedSizes[sizeName] = glm::uvec3(newSize, 1);

	execCounter = 0;

	for (size_t i = 0; i < 3; i++)
		gfxCommandPools.push_back(renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT | COMMAND_POOL_RESET_COMMAND_BUFFER_BIT));

	for (size_t i = 0; i < 3; i++)
		gfxCommandBuffers.push_back(gfxCommandPools[i]->allocateCommandBuffer());

	for (size_t i = 0; i < 3; i++)
		executionDoneSemaphores.push_back(renderer->createSemaphore());

	build();
}

void D3D12RenderGraph::cleanupResources()
{
	for (auto it = graphTextureViews.begin(); it != graphTextureViews.end(); it++)
		renderer->destroyTextureView(it->second);

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

	} PhysicalResourceData;

	std::map<std::string, PhysicalResourceData> resourceData;

	// Gather information about each resource and how it will be used
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RenderGraphRenderPass &pass = *passes[passStack[i]];

		for (size_t i = 0; i < pass.getSampledTextureInputs().size(); i++)
			graphTextureViewsInitialResourceState[pass.getSampledTextureInputs()[i]] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(graphTextureViews[pass.getSampledTextureInputs()[i]]->viewFormat) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);

		for (size_t i = 0; i < pass.getInputAttachmentInputs().size(); i++)
			graphTextureViewsInitialResourceState[pass.getInputAttachmentInputs()[i]] = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(graphTextureViews[pass.getInputAttachmentInputs()[i]]->viewFormat) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON);

		for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
		{
			PhysicalResourceData data = {};
			data.attachment = pass.getColorAttachmentOutputs()[o].attachment;
			data.usageFlags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
			data.clearValue = pass.getColorAttachmentOutputs()[o].attachmentClearValue;

			const std::string &textureName = pass.getColorAttachmentOutputs()[o].textureName;

			resourceData[textureName] = data;
			graphTextureViewsInitialResourceState[textureName] = outputAttachmentName == textureName ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : D3D12_RESOURCE_STATE_RENDER_TARGET;
		}

		if (pass.hasDepthStencilOutput())
		{
			PhysicalResourceData data = {};
			data.attachment = pass.getDepthStencilAttachmentOutput().attachment;
			data.usageFlags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			data.clearValue = pass.getDepthStencilAttachmentOutput().attachmentClearValue;
			
			const std::string &textureName = pass.getDepthStencilAttachmentOutput().textureName;

			resourceData[textureName] = data;
			graphTextureViewsInitialResourceState[textureName] = outputAttachmentName == textureName ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;
		}

		for (size_t o = 0; o < pass.getStorageTextures().size(); o++)
		{
			const std::string &textureName = pass.getStorageTextures()[o].textureName;

			if (pass.getStorageTextures()[o].canWriteAsOutput)
			{
				PhysicalResourceData data = {};
				data.attachment = pass.getStorageTextures()[o].attachment;
				data.usageFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
				data.clearValue = {0.0f, 0.0f, 0.0f, 0.0f};

				resourceData[textureName] = data;
				graphTextureViewsInitialResourceState[textureName] = outputAttachmentName == textureName ? (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | (isDepthFormat(data.attachment.format) ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COMMON)) : TextureLayoutToD3D12ResourceStates(pass.getStorageTextures()[o].passEndLayout);
			}
			else
			{
				PhysicalResourceData &data = resourceData[textureName];
				data.usageFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
			}
		}
	}

	// Actually create all the resources
	for (auto it = resourceData.begin(); it != resourceData.end(); it++)
	{
		const PhysicalResourceData &data = it->second;

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
		texDesc.SampleDesc = {1, 0};
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = data.usageFlags;

		D3D12RenderGraphTexture graphTexture = {};
		graphTexture.attachment = it->second.attachment;
		graphTexture.rendererTexture = new D3D12Texture();
		graphTexture.rendererTexture->width = sizeX;
		graphTexture.rendererTexture->height = sizeY;
		graphTexture.rendererTexture->depth = 1;
		graphTexture.rendererTexture->textureFormat = data.attachment.format;
		graphTexture.rendererTexture->layerCount = data.attachment.arrayLayers;
		graphTexture.rendererTexture->mipCount = data.attachment.mipLevels;

		D3D12_RESOURCE_STATES textureState = graphTextureViewsInitialResourceState[it->first];

		bool useClearValue = (texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) || (texDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

		D3D12_CLEAR_VALUE optClearValue = {};
		optClearValue.Format = texDesc.Format;
		memcpy(optClearValue.Color, data.clearValue.color.float32, sizeof(optClearValue.Color));

		DX_CHECK_RESULT(renderer->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc, textureState, useClearValue ? &optClearValue : nullptr, IID_PPV_ARGS(&graphTexture.rendererTexture->textureResource)));

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

		pass.getInitFunction()(initData);
	}

	typedef struct
	{
		D3D12_RESOURCE_STATES currentState;

	} RenderPassResourceState;

	std::map<std::string, RenderPassResourceState> resourceStates;

	/*
	Initialize all resources with the last state they would be in after a full render graph execution,
	as this will be the first state we transition out of when we execute it again
	*/
	for (auto initialResourceStatesIt = graphTextureViewsInitialResourceState.begin(); initialResourceStatesIt != graphTextureViewsInitialResourceState.end(); initialResourceStatesIt++)
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
				transitionBarrier.pResource = static_cast<D3D12Texture*>(graphTextureViews[inputName]->parentTexture)->textureResource;
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
				transitionBarrier.pResource = static_cast<D3D12Texture*>(graphTextureViews[inputName]->parentTexture)->textureResource;
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
				transitionBarrier.pResource = static_cast<D3D12Texture*>(graphTextureViews[storageTextureName]->parentTexture)->textureResource;
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
				transitionBarrier.pResource = static_cast<D3D12Texture*>(graphTextureViews[storageTextureName]->parentTexture)->textureResource;
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

		for (size_t o = 0; o < pass.getColorAttachmentOutputs().size(); o++)
		{
			const std::string &outputName = pass.getColorAttachmentOutputs()[o].textureName;
			D3D12_RESOURCE_STATES inputCurrentState = resourceStates[outputName].currentState;
			D3D12_RESOURCE_STATES inputRequiredState = D3D12_RESOURCE_STATE_RENDER_TARGET;

			if (inputCurrentState != inputRequiredState)
			{
				D3D12_RESOURCE_TRANSITION_BARRIER transitionBarrier = {};
				transitionBarrier.pResource = static_cast<D3D12Texture*>(graphTextureViews[outputName]->parentTexture)->textureResource;
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
				transitionBarrier.pResource = static_cast<D3D12Texture*>(graphTextureViews[outputName]->parentTexture)->textureResource;
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
				transitionBarrier.pResource = static_cast<D3D12Texture*>(graphTextureViews[outputName]->parentTexture)->textureResource;
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
				transitionBarrier.pResource = static_cast<D3D12Texture*>(graphTextureViews[outputName]->parentTexture)->textureResource;
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

	RenderGraphDescriptorUpdateFunctionData descUpdateData = {};
	descUpdateData.graphTextureViews = graphTextureViews;

	for (size_t i = 0; i < finalPasses.size(); i++)
		passes[finalPasses[i].passIndex]->getDescriptorUpdateFunction()(descUpdateData);
}

#endif