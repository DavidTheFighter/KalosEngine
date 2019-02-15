#ifndef RENDERERCORE_D3D12_D3D12PIPELINEHELPER_H_
#define RENDERERCORE_D3D12_D3D12PIPELINEHELPER_H_

#include <RendererCore/D3D12/d3d12_common.h>
#if BUILD_D3D12_BACKEND

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class D3D12Renderer;

class D3D12PipelineHelper
{
	public:

	D3D12PipelineHelper(D3D12Renderer *rendererPtr);
	virtual ~D3D12PipelineHelper();

	Pipeline createGraphicsPipeline(const GraphicsPipelineInfo &pipelineInfo, RenderPass renderPass, uint32_t subpass);
	Pipeline createComputePipeline(const ComputePipelineInfo &pipelineInfo);

	private:

	D3D12Renderer *renderer;
};

#endif
#endif /* RENDERERCORE_D3D12_D3D12PIPELINEHELPER_H_ */