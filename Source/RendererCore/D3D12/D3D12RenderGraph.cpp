#include "RendererCore/D3D12/D3D12RenderGraph.h"

#include <RendererCore/Renderer.h>
#include <RendererCore/D3D12/D3D12Renderer.h>

#include <RendererCore/RendererObjects.h>

#include <RendererCore/D3D12/D3D12Enums.h>
#include <RendererCore/D3D12/D3D12Objects.h>

D3D12RenderGraph::D3D12RenderGraph(D3D12Renderer *rendererPtr)
{
	renderer = rendererPtr;
	d3drenderer = rendererPtr;

	execCounter = 0;

	enableSubpassMerging = false;

	for (size_t i = 0; i < 3; i ++)
		gfxCommandPools.push_back(renderer->createCommandPool(QUEUE_TYPE_GRAPHICS, COMMAND_POOL_TRANSIENT_BIT | COMMAND_POOL_RESET_COMMAND_BUFFER_BIT));

	for (size_t i = 0; i < 3; i++)
		executionDoneSemaphores.push_back(renderer->createSemaphore());

	for (size_t i = 0; i < 3; i++)
		gfxCommandBuffers.push_back(gfxCommandPools[i]->allocateCommandBuffer());

	rtvDescriptorSize = d3drenderer->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
}

D3D12RenderGraph::~D3D12RenderGraph()
{
	cleanupResources();
}

Semaphore D3D12RenderGraph::execute()
{
	D3D12CommandBuffer *cmdBuffer = static_cast<D3D12CommandBuffer*>(gfxCommandBuffers[execCounter % gfxCommandBuffers.size()]);
	gfxCommandPools[execCounter % gfxCommandPools.size()]->resetCommandPool();

	cmdBuffer->beginCommands(COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	for (size_t i = 0; i < finalPassStack.size(); i++)
	{
		RendererGraphRenderPass &pass = *passes[finalPassStack[i]];
		const RenderPassAttachment &exAtt = pass.hasDepthStencilOutput() ? pass.getDepthStencilOutput().second.attachment : pass.getColorOutputs()[0].second.attachment;

		std::vector<D3D12_RESOURCE_BARRIER> barriers;

		for (size_t o = 0; o < pass.getColorOutputs().size(); o++)
		{
			D3D12_RESOURCE_TRANSITION_BARRIER transBarrier = {};
			transBarrier.pResource = static_cast<D3D12TextureView*>(graphTextureViews[pass.getColorOutputs()[o].first])->parentTextureResource;
			transBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transBarrier.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			transBarrier.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition = transBarrier;

			barriers.push_back(barrier);
		}

		if (pass.hasDepthStencilOutput())
		{
			D3D12_RESOURCE_TRANSITION_BARRIER transBarrier = {};
			transBarrier.pResource = static_cast<D3D12TextureView*>(graphTextureViews[pass.getDepthStencilOutput().first])->parentTextureResource;
			transBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transBarrier.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;
			transBarrier.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition = transBarrier;

			barriers.push_back(barrier);
		}

		cmdBuffer->d3d12_resourceBarrier(barriers);
		barriers.clear();

		for (size_t o = 0; o < pass.getColorOutputs().size(); o++)
			if (pass.getColorOutputs()[o].second.clearAttachment)
				cmdBuffer->d3d12_clearRenderTargetView({graphRTVs[finalPassStack[i]]->GetCPUDescriptorHandleForHeapStart().ptr + rtvDescriptorSize * o}, pass.getColorOutputs()[o].second.attachmentClearValue);

		if (pass.hasDepthStencilOutput())
			if (pass.getDepthStencilOutput().second.clearAttachment)
				cmdBuffer->d3d12_clearDepthStencilView(graphDSVs[finalPassStack[i]]->GetCPUDescriptorHandleForHeapStart(), pass.getDepthStencilOutput().second.attachmentClearValue.depthStencil.depth, pass.getDepthStencilOutput().second.attachmentClearValue.depthStencil.stencil);

		if (pass.getColorOutputs().size() > 0)
			cmdBuffer->d3d12_OMSetRenderTargets({graphRTVs[finalPassStack[i]]->GetCPUDescriptorHandleForHeapStart()}, true, graphDSVs[finalPassStack[i]] != nullptr ? &graphDSVs[finalPassStack[i]]->GetCPUDescriptorHandleForHeapStart() : nullptr);
		else
			cmdBuffer->d3d12_OMSetRenderTargets({}, false, &graphDSVs[finalPassStack[i]]->GetCPUDescriptorHandleForHeapStart());

		uint32_t sizeX = exAtt.namedRelativeSize == "" ? uint32_t(exAtt.sizeX) : uint32_t(exAtt.sizeX * namedSizes[exAtt.namedRelativeSize].x);
		uint32_t sizeY = exAtt.namedRelativeSize == "" ? uint32_t(exAtt.sizeY) : uint32_t(exAtt.sizeY * namedSizes[exAtt.namedRelativeSize].y);

		cmdBuffer->setViewports(0, {{0, 0, (float) sizeX, (float) sizeY, 0, 1}});
		cmdBuffer->setScissors(0, { {0, 0, sizeX, sizeY} });

		RenderGraphRenderFunctionData renderData = {};

		pass.getRenderFunction()(cmdBuffer, renderData);

		for (size_t o = 0; o < pass.getColorOutputs().size(); o++)
		{
			D3D12_RESOURCE_TRANSITION_BARRIER transBarrier = {};
			transBarrier.pResource = static_cast<D3D12TextureView*>(graphTextureViews[pass.getColorOutputs()[o].first])->parentTextureResource;
			transBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transBarrier.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			transBarrier.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition = transBarrier;

			barriers.push_back(barrier);
		}

		if (pass.hasDepthStencilOutput())
		{
			D3D12_RESOURCE_TRANSITION_BARRIER transBarrier = {};
			transBarrier.pResource = static_cast<D3D12TextureView*>(graphTextureViews[pass.getDepthStencilOutput().first])->parentTextureResource;
			transBarrier.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			transBarrier.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
			transBarrier.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ;

			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition = transBarrier;

			barriers.push_back(barrier);
		}

		cmdBuffer->d3d12_resourceBarrier(barriers);
	}

	cmdBuffer->endCommands();

	std::vector<Semaphore> waitSems = {};
	std::vector<PipelineStageFlags> waitSemStages = {};
	std::vector<Semaphore> signalSems = {executionDoneSemaphores[execCounter % executionDoneSemaphores.size()]};

	renderer->submitToQueue(QUEUE_TYPE_GRAPHICS, {cmdBuffer}, waitSems, waitSemStages, {});

	execCounter++;

	return signalSems[0];
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
		graphTextures[i].textureHeap->Release();

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

	finalPassStack.clear();
	executionDoneSemaphores.clear();
	gfxCommandPools.clear();
	gfxCommandBuffers.clear();
	graphTextures.clear();
	allAttachments.clear();
	graphTextureViews.clear();
	graphRTVs.clear();
	graphDSVs.clear();

	attachmentUsageLifetimes.clear();
	attachmentAliasingCandidates.clear();
}

TextureView D3D12RenderGraph::getRenderGraphOutputTextureView()
{
	return graphTextureViews[renderGraphOutputAttachment];
}

const std::string viewSelectMipPostfix = "_fg_miplvl";
const std::string viewSelectLayerPostfix = "_fg_layer";

void D3D12RenderGraph::assignPhysicalResources(const std::vector<size_t>& passStack)
{
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RendererGraphRenderPass &pass = *passes[passStack[i]];

		for (size_t o = 0; o < pass.getColorOutputs().size(); o++)
			allAttachments.emplace(pass.getColorOutputs()[o]);

		if (pass.hasDepthStencilOutput())
			allAttachments.emplace(pass.getDepthStencilOutput());
	}

	for (auto it = allAttachments.begin(); it != allAttachments.end(); it++)
	{
		uint32_t sizeX = it->second.attachment.namedRelativeSize == "" ? uint32_t(it->second.attachment.sizeX) : uint32_t(namedSizes[it->second.attachment.namedRelativeSize].x * it->second.attachment.sizeX);
		uint32_t sizeY = it->second.attachment.namedRelativeSize == "" ? uint32_t(it->second.attachment.sizeY) : uint32_t(namedSizes[it->second.attachment.namedRelativeSize].y * it->second.attachment.sizeY);

		D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

		if (isDepthFormat(it->second.attachment.format))
			flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

		D3D12_RESOURCE_DESC texDesc = {};
		texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		texDesc.Alignment = 0;
		texDesc.Width = sizeX;
		texDesc.Height = sizeY;
		texDesc.DepthOrArraySize = it->second.attachment.arrayLayers;
		texDesc.MipLevels = it->second.attachment.mipLevels;
		texDesc.Format = ResourceFormatToDXGIFormat(it->second.attachment.format);
		texDesc.SampleDesc = {1, 0};
		texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		texDesc.Flags = flags;

		printf("rg is %u\n", ResourceFormatToDXGIFormat(it->second.attachment.format));

		D3D12RenderGraphTexture graphTexture = {};
		graphTexture.attachment = it->second.attachment;

		D3D12_RESOURCE_STATES textureState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		if (isDepthFormat(it->second.attachment.format))
			textureState |= D3D12_RESOURCE_STATE_DEPTH_READ;

		DX_CHECK_RESULT(d3drenderer->device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE, &texDesc, textureState, nullptr, IID_PPV_ARGS(&graphTexture.textureHeap)));

		graphTextures.push_back(graphTexture);

		D3D12TextureView *graphTextureView = new D3D12TextureView();
		graphTextureView->parentTexture = nullptr;
		graphTextureView->parentTextureResource = graphTexture.textureHeap;
		graphTextureView->viewType = TEXTURE_VIEW_TYPE_2D;
		graphTextureView->viewFormat = it->second.attachment.format;
		graphTextureView->baseMip = 0;
		graphTextureView->mipCount = it->second.attachment.mipLevels;
		graphTextureView->baseLayer = 0;
		graphTextureView->layerCount = it->second.attachment.arrayLayers;

		graphTextureViews[it->first] = graphTextureView;

		for (uint32_t m = 0; m < it->second.attachment.mipLevels; m++)
		{
			graphTextureView = new D3D12TextureView();
			graphTextureView->parentTexture = nullptr;
			graphTextureView->parentTextureResource = graphTexture.textureHeap;
			graphTextureView->viewType = TEXTURE_VIEW_TYPE_2D;
			graphTextureView->viewFormat = it->second.attachment.format;
			graphTextureView->baseMip = m;
			graphTextureView->mipCount = 1;
			graphTextureView->baseLayer = 0;
			graphTextureView->layerCount = it->second.attachment.arrayLayers;

			graphTextureViews[it->first + viewSelectMipPostfix + toString(m)] = graphTextureView;
		}

		for (uint32_t l = 0; l < it->second.attachment.arrayLayers; l++)
		{
			graphTextureView = new D3D12TextureView();
			graphTextureView->parentTexture = nullptr;
			graphTextureView->parentTextureResource = graphTexture.textureHeap;
			graphTextureView->viewType = TEXTURE_VIEW_TYPE_2D;
			graphTextureView->viewFormat = it->second.attachment.format;
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
	for (size_t i = 0; i < passStack.size(); i++)
	{
		RendererGraphRenderPass &pass = *passes[passStack[i]].get();

		ID3D12DescriptorHeap *rtvHeap = nullptr, *dsvHeap = nullptr;

		if (pass.getColorOutputs().size() > 0)
		{
			D3D12_DESCRIPTOR_HEAP_DESC passRTVDesc = {};
			passRTVDesc.NumDescriptors = pass.getColorOutputs().size();
			passRTVDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

			DX_CHECK_RESULT(d3drenderer->device->CreateDescriptorHeap(&passRTVDesc, IID_PPV_ARGS(&rtvHeap)));

			for (size_t o = 0; o < passRTVDesc.NumDescriptors; o++)
				d3drenderer->device->CreateRenderTargetView(static_cast<D3D12TextureView*>(graphTextureViews[pass.getColorOutputs()[o].first])->parentTextureResource, nullptr, {rtvHeap->GetCPUDescriptorHandleForHeapStart().ptr + rtvDescriptorSize * o});
		}

		if (pass.hasDepthStencilOutput())
		{
			D3D12_DESCRIPTOR_HEAP_DESC passDSVDesc = {};
			passDSVDesc.NumDescriptors = 1;
			passDSVDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
			passDSVDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			
			DX_CHECK_RESULT(d3drenderer->device->CreateDescriptorHeap(&passDSVDesc, IID_PPV_ARGS(&dsvHeap)));

			d3drenderer->device->CreateDepthStencilView(static_cast<D3D12TextureView*>(graphTextureViews[pass.getDepthStencilOutput().first])->parentTextureResource, nullptr, dsvHeap->GetCPUDescriptorHandleForHeapStart());
		}

		graphRTVs[passStack[i]] = rtvHeap;
		graphDSVs[passStack[i]] = dsvHeap;

		std::vector<AttachmentDescription> tempAttDesc;
		std::vector<AttachmentReference> tempAttRefs;

		for (size_t a = 0; a < pass.getColorOutputs().size(); a++)
		{
			AttachmentDescription desc = {};
			desc.format = pass.getColorOutputs()[a].second.attachment.format;

			tempAttDesc.push_back(desc);

			AttachmentReference ref = {};
			ref.attachment = a;

			tempAttRefs.push_back(ref);
		}

		SubpassDescription tempSubpassDesc = {};
		tempSubpassDesc.colorAttachments = tempAttRefs;

		std::unique_ptr<RendererRenderPass> tempRenderPass(new RendererRenderPass());
		tempRenderPass->attachments = tempAttDesc;
		tempRenderPass->subpasses.push_back(tempSubpassDesc);

		RenderGraphInitFunctionData initData = {};
		initData.baseSubpass = 0;
		initData.renderPassHandle = tempRenderPass.get();

		pass.getInitFunction()(initData);
	}

	finalPassStack = passStack;
}
