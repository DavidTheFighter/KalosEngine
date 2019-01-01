#ifndef RENDERERCORE_RENDERERGRAPHRENDERPASS_H_
#define RENDERERCORE_RENDERERGRAPHRENDERPASS_H_

#include <common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

typedef struct RenderPassAttachment
{
	/*
	Allows this attachment to have a size that is relative to a "named size" defined in the frame graph. This can be used for keeping a bunch of
	attachments the same size as the swapchain, or half the swapchain resolution. Essentially you can provide a single size, and have multiple 
	attachments that keep that size and update when that size is updated. If "namedRelativeSize" is an empty string, it is its own independent
	resolution as defined by (sizeX, sizeY, sizeZ). When is it's relative, then (sizeX, sizeY, sizeZ) are all multipliers of that relative size.
	*/
	std::string namedRelativeSize = "";
	ResourceFormat format;
	float sizeX = 1;
	float sizeY = 1;
	float sizeZ = 1;
	uint32_t mipLevels = 1;
	uint32_t arrayLayers = 1;
	TextureViewType viewType = TEXTURE_VIEW_TYPE_2D;

	bool operator==(const RenderPassAttachment &other) const
	{
		return namedRelativeSize == other.namedRelativeSize && format == other.format && sizeX == other.sizeX && sizeY == other.sizeY && sizeZ == other.sizeZ && mipLevels == other.mipLevels && arrayLayers == other.arrayLayers && viewType == other.viewType;
	}

	bool operator<(const RenderPassAttachment &other) const
	{
		return namedRelativeSize < other.namedRelativeSize || format < other.format || sizeX < other.sizeX || sizeY < other.sizeY || sizeZ < other.sizeZ || mipLevels < other.mipLevels || arrayLayers < other.arrayLayers || viewType < other.viewType;
	}

} RenderPassAttachment;

typedef struct
{
	RenderPassAttachment attachment;

	bool clearAttachment;
	bool genMipsWithPostBlit;

	ClearValue attachmentClearValue;

} RenderPassOutputAttachment;

typedef struct
{
	RenderPass renderPassHandle;
	uint32_t baseSubpass;
} RenderGraphInitFunctionData;

typedef struct
{

} RenderGraphDescriptorUpdateFunctionData;

typedef struct
{

} RenderGraphRenderFunctionData;

class RendererGraphRenderPass
{
	public:

	RendererGraphRenderPass(const std::string &passName, uint32_t pipelineType);
	virtual ~RendererGraphRenderPass();

	void addColorOutput(const std::string &name, const RenderPassAttachment &info, bool clearAttachment = true, ClearValue textureClearValue = {0, 0, 0, 0}, bool genMipsWithPostBlit = false);
	void setDepthStencilOutput(const std::string &name, const RenderPassAttachment &info, bool clearAttachment = true, ClearValue textureClearValue = {1, 0}, bool genMipsWithPostBlit = false);

	void addColorInput(const std::string &name);
	void addDepthStencilInput(const std::string &name);

	void addColorFragmentInputAttachment(const std::string &name);
	void addDepthStencilFragmentInputAttachment(const std::string &name);

	void setInitFunction(std::function<void(const RenderGraphInitFunctionData&)> callbackFunc);
	//void setDescriptorUpdateFunction(std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> updateFunc);
	void setRenderFunction(std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> renderFunc);

	std::function<void(const RenderGraphInitFunctionData&)> getInitFunction();
	//std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> getDescriptorUpdateFunction();
	std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> getRenderFunction();

	std::string getNodeName() const;
	uint32_t getPipelineType() const;

	bool hasDepthStencilOutput() const;

	const std::vector<std::pair<std::string, RenderPassOutputAttachment>> &getColorOutputs() const;
	const std::pair<std::string, RenderPassOutputAttachment> &getDepthStencilOutput() const;

	const std::vector<std::string> &getColorInputs() const;
	const std::vector<std::string> &getColorFragmentInputAttachments() const;
	const std::vector<std::string> &getDepthStencilInputs() const;
	const std::vector<std::string> &getDepthStencilFragmentInputAttachments() const;

	private:

	std::string nodeName;
	uint32_t pipelineType;

	std::vector<std::pair<std::string, RenderPassOutputAttachment>> colorOutputs;
	std::pair<std::string, RenderPassOutputAttachment> depthStencilOutput;

	std::vector<std::string> colorInputs;
	std::vector<std::string> depthStencilInputs;
	std::vector<std::string> colorInputAttachments;
	std::vector<std::string> depthStencilInputAttachments;

	std::function<void(const RenderGraphInitFunctionData&)> initFunction;
	//std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> descriptorUpdateFunction;
	std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> renderFunction;
};

#endif /* RENDERERCORE_FRAMEGRAPHRENDERPASS_H_ */