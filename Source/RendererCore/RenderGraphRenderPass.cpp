#include "RendererCore/RenderGraphRenderPass.h"

RenderGraphRenderPass::RenderGraphRenderPass(const std::string &passName, RenderGraphPipelineType pipelineType)
{
	this->passName = passName;
	this->pipelineType = pipelineType;
	this->hasDepthStencilAttachment = false;

	hasInitFunc = false;
	hasDescUpdateFunc = false;
	hasRenderFunc = false;
}

RenderGraphRenderPass::~RenderGraphRenderPass()
{

}

void RenderGraphRenderPass::addColorAttachmentOutput(const std::string &textureName, const RenderPassAttachment &info, bool clearAttachment, ClearValue textureClearValue)
{
	RenderPassOutputAttachment attachment = {};
	attachment.textureName = textureName;
	attachment.attachment = info;
	attachment.clearAttachment = clearAttachment;
	attachment.attachmentClearValue = textureClearValue;

	colorAttachmentOutputs.push_back(attachment);
}

void RenderGraphRenderPass::setDepthStencilAttachmentOutput(const std::string &textureName, const RenderPassAttachment &info, bool clearAttachment, ClearValue textureClearValue)
{
	RenderPassOutputAttachment attachment = {};
	attachment.textureName = textureName;
	attachment.attachment = info;
	attachment.clearAttachment = clearAttachment;
	attachment.attachmentClearValue = textureClearValue;

	depthStencilAttachmentOutput = attachment;

	hasDepthStencilAttachment = true;
}

void RenderGraphRenderPass::addSampledTextureInput(const std::string &name)
{
	sampledTextureInputs.push_back(name);
}

void RenderGraphRenderPass::addInputAttachmentInput(const std::string &name)
{
	inputAttachmentInputs.push_back(name);
}

void RenderGraphRenderPass::addStorageTexture(const std::string &name, const RenderPassAttachment &info, bool canReadAsInput, bool canWriteAsOutput, TextureLayout passBeginLayout, TextureLayout passEndLayout)
{
	RenderPassStorageTexture storageTexture = {};
	storageTexture.textureName = name;
	storageTexture.attachment = info;
	storageTexture.canReadAsInput = canReadAsInput;
	storageTexture.canWriteAsOutput = canWriteAsOutput;
	storageTexture.passBeginLayout = passBeginLayout;
	storageTexture.passEndLayout = passEndLayout;

	storageTextures.push_back(storageTexture);
}

void RenderGraphRenderPass::addStorageBuffer(const std::string &bufferName, uint32_t size, BufferUsageFlags usage, bool canReadAsInput, bool canWriteAsOutput, BufferLayout passBeginLayout, BufferLayout passEndLayout)
{
	RenderPassStorageBuffer storageBuffer = {};
	storageBuffer.bufferName = bufferName;
	storageBuffer.size = size;
	storageBuffer.usage = usage;
	storageBuffer.canReadAsInput = canReadAsInput;
	storageBuffer.canWriteAsOutput = canWriteAsOutput;
	storageBuffer.passBeginLayout = passBeginLayout;
	storageBuffer.passEndLayout = passEndLayout;

	storageBuffers.push_back(storageBuffer);
}

void RenderGraphRenderPass::setInitFunction(std::function<void(const RenderGraphInitFunctionData&)> callbackFunc)
{
	initFunction = callbackFunc;
	hasInitFunc = true;
}

void RenderGraphRenderPass::setDescriptorUpdateFunction(std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> updateFunc)
{
	descriptorUpdateFunction = updateFunc;
	hasDescUpdateFunc = true;
}

void RenderGraphRenderPass::setRenderFunction(std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> renderFunc)
{
	renderFunction = renderFunc;
	hasRenderFunc = true;
}

std::function<void(const RenderGraphInitFunctionData&)> RenderGraphRenderPass::getInitFunction()
{
	return initFunction;
}

std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> RenderGraphRenderPass::getDescriptorUpdateFunction()
{
	return descriptorUpdateFunction;
}

std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> RenderGraphRenderPass::getRenderFunction()
{
	return renderFunction;
}

bool RenderGraphRenderPass::hasInitFunction() const
{
	return hasInitFunc;
}

bool RenderGraphRenderPass::hasDescriptorUpdateFunction() const
{
	return hasDescUpdateFunc;
}

bool RenderGraphRenderPass::hasRenderFunction() const
{
	return hasRenderFunc;
}

bool RenderGraphRenderPass::hasDepthStencilOutput() const
{
	return hasDepthStencilAttachment;
}

const std::vector<RenderPassOutputAttachment> &RenderGraphRenderPass::getColorAttachmentOutputs() const
{
	return colorAttachmentOutputs;
}

const RenderPassOutputAttachment &RenderGraphRenderPass::getDepthStencilAttachmentOutput() const
{
	return depthStencilAttachmentOutput;
}

const std::vector<std::string> &RenderGraphRenderPass::getSampledTextureInputs() const
{
	return sampledTextureInputs;
}

const std::vector<std::string> &RenderGraphRenderPass::getInputAttachmentInputs() const
{
	return inputAttachmentInputs;
}

const std::vector<RenderPassStorageTexture> &RenderGraphRenderPass::getStorageTextures() const
{
	return storageTextures;
}

const std::vector<RenderPassStorageBuffer> &RenderGraphRenderPass::getStorageBuffers() const
{
	return storageBuffers;
}

const std::string &RenderGraphRenderPass::getPassName() const
{
	return passName;
}

RenderGraphPipelineType RenderGraphRenderPass::getPipelineType() const
{
	return pipelineType;
}