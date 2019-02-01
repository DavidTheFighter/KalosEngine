#include "RendererCore/RendererGraphRenderPass.h"



RendererGraphRenderPass::RendererGraphRenderPass(const std::string &passName, uint32_t pipelineType)
{
	nodeName = passName;
	this->pipelineType = pipelineType;
}


RendererGraphRenderPass::~RendererGraphRenderPass()
{

}

void RendererGraphRenderPass::addColorOutput(const std::string &name, const RenderPassAttachment &info, bool clearAttachment, ClearValue textureClearValue, bool genMipsWithPostBlit)
{
	if (isDepthFormat(info.format))
	{
		Log::get()->error("RendererGraphRenderPass: Can\'t set attachment \"{}\" as a color output, format {} is a depth/stencil format", name, info.format);
		return;
	}

	RenderPassOutputAttachment outputAttachment = {};
	outputAttachment.attachment = info;
	outputAttachment.clearAttachment = clearAttachment;
	outputAttachment.genMipsWithPostBlit = genMipsWithPostBlit;
	outputAttachment.attachmentClearValue = textureClearValue;

	colorOutputs.push_back(std::make_pair(name, outputAttachment));
}

void RendererGraphRenderPass::setDepthStencilOutput(const std::string &name, const RenderPassAttachment &info, bool clearAttachment, ClearValue textureClearValue, bool genMipsWithPostBlit)
{
	if (!isDepthFormat(info.format))
	{
		Log::get()->error("RendererGraphRenderPass: Can\'t set attachment \"{}\" as a depth/stencil output, format {} is NOT a depth/stencil format", name, info.format);
		return;
	}

	RenderPassOutputAttachment outputAttachment = {};
	outputAttachment.attachment = info;
	outputAttachment.clearAttachment = clearAttachment;
	outputAttachment.genMipsWithPostBlit = genMipsWithPostBlit;
	outputAttachment.attachmentClearValue = textureClearValue;

	depthStencilOutput = std::make_pair(name, outputAttachment);
}

void RendererGraphRenderPass::addColorInput(const std::string &name)
{
	colorInputs.push_back(name);
}

void RendererGraphRenderPass::addDepthStencilInput(const std::string &name)
{
	depthStencilInputs.push_back(name);
}

void RendererGraphRenderPass::addColorFragmentInputAttachment(const std::string &name)
{
	colorInputAttachments.push_back(name);
}

void RendererGraphRenderPass::addDepthStencilFragmentInputAttachment(const std::string &name)
{
	depthStencilInputAttachments.push_back(name);
}

void RendererGraphRenderPass::setInitFunction(std::function<void(const RenderGraphInitFunctionData&)> callbackFunc)
{
	initFunction = callbackFunc;
}

void RendererGraphRenderPass::setDescriptorUpdateFunction(std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> updateFunc)
{
	descriptorUpdateFunction = updateFunc;
}

void RendererGraphRenderPass::setRenderFunction(std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> renderFunc)
{
	renderFunction = renderFunc;
}

std::function<void(const RenderGraphInitFunctionData&)> RendererGraphRenderPass::getInitFunction()
{
	return initFunction;
}

std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> RendererGraphRenderPass::getDescriptorUpdateFunction()
{
	return descriptorUpdateFunction;
}

std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> RendererGraphRenderPass::getRenderFunction()
{
	return renderFunction;
}

std::string RendererGraphRenderPass::getNodeName() const
{
	return nodeName;
}

uint32_t RendererGraphRenderPass::getPipelineType() const
{
	return pipelineType;
}

bool RendererGraphRenderPass::hasDepthStencilOutput() const
{
	return !depthStencilOutput.first.empty();
}

const std::vector<std::pair<std::string, RenderPassOutputAttachment>> &RendererGraphRenderPass::getColorOutputs() const
{
	return colorOutputs;
}

const std::pair<std::string, RenderPassOutputAttachment> &RendererGraphRenderPass::getDepthStencilOutput() const
{
	return depthStencilOutput;
}

const std::vector<std::string> &RendererGraphRenderPass::getColorInputs() const
{
	return colorInputs;
}

const std::vector<std::string> &RendererGraphRenderPass::getColorFragmentInputAttachments() const
{
	return colorInputAttachments;
}

const std::vector<std::string> &RendererGraphRenderPass::getDepthStencilInputs() const
{
	return depthStencilInputs;
}

const std::vector<std::string> &RendererGraphRenderPass::getDepthStencilFragmentInputAttachments() const
{
	return depthStencilInputAttachments;
}