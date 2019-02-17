#ifndef RENDERERCORE_RENDERGRAPHRENDERPASS_H_
#define RENDERERCORE_RENDERGRAPHRENDERPASS_H_

#include <RendererCore/renderer_common.h>
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
	std::string textureName;
	RenderPassAttachment attachment;

	bool clearAttachment;
	ClearValue attachmentClearValue;

} RenderPassOutputAttachment;

typedef struct
{
	std::string textureName;
	RenderPassAttachment attachment;

	bool canReadAsInput;
	bool canWriteAsOutput;
	TextureLayout passBeginLayout; // The layout this texture should be when the render function of this pass is called
	TextureLayout passEndLayout; // The layout that this texture will be in when the render function of this pass ends (should not change unless it is changed manually within the render func)

} RenderPassStorageTexture;

typedef struct
{
	RenderPass renderPassHandle;
	uint32_t baseSubpass;
} RenderGraphInitFunctionData;

typedef struct
{
	std::map<std::string, TextureView> graphTextureViews;
} RenderGraphDescriptorUpdateFunctionData;

typedef struct
{

} RenderGraphRenderFunctionData;

class RenderGraphRenderPass
{
	public:

	RenderGraphRenderPass(const std::string &passName, RenderGraphPipelineType pipelineType);
	virtual ~RenderGraphRenderPass();

	void addColorAttachmentOutput(const std::string &textureName, const RenderPassAttachment &info, bool clearAttachment = false, ClearValue textureClearValue = {0, 0, 0, 1});
	void setDepthStencilAttachmentOutput(const std::string &textureName, const RenderPassAttachment &info, bool clearAttachment = false, ClearValue textureClearValue = {1, 0});

	void addSampledTextureInput(const std::string &name);
	void addInputAttachmentInput(const std::string &name);

	void addStorageTexture(const std::string &name, const RenderPassAttachment &info, bool canReadAsInput, bool canWriteAsOutput);

	void setInitFunction(std::function<void(const RenderGraphInitFunctionData&)> callbackFunc);
	void setDescriptorUpdateFunction(std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> updateFunc);
	void setRenderFunction(std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> renderFunc);

	std::function<void(const RenderGraphInitFunctionData&)> getInitFunction();
	std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> getDescriptorUpdateFunction();
	std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> getRenderFunction();

	bool hasInitFunction() const;
	bool hasDescriptorUpdateFunction() const;
	bool hasRenderFunction() const;

	bool hasDepthStencilOutput() const;

	const std::vector<RenderPassOutputAttachment> &getColorAttachmentOutputs() const;
	const RenderPassOutputAttachment &getDepthStencilAttachmentOutput() const;

	const std::vector<std::string> &getSampledTextureInputs() const;
	const std::vector<std::string> &getInputAttachmentInputs() const;

	const std::vector<RenderPassStorageTexture> &getStorageTextures() const;

	const std::string &getPassName() const;
	RenderGraphPipelineType getPipelineType() const;

	private:

	std::string passName;
	RenderGraphPipelineType pipelineType;
	bool hasDepthStencilAttachment;

	std::vector<RenderPassOutputAttachment> colorAttachmentOutputs;
	RenderPassOutputAttachment depthStencilAttachmentOutput;

	std::vector<std::string> sampledTextureInputs;
	std::vector<std::string> inputAttachmentInputs;

	std::vector<RenderPassStorageTexture> storageTextures;

	std::function<void(const RenderGraphInitFunctionData&)> initFunction;
	std::function<void(const RenderGraphDescriptorUpdateFunctionData&)> descriptorUpdateFunction;
	std::function<void(CommandBuffer cmdBuffer, const RenderGraphRenderFunctionData&)> renderFunction;

	bool hasInitFunc;
	bool hasDescUpdateFunc;
	bool hasRenderFunc;
};

#endif /* RENDERERCORE_RENDERGRAPHRENDERPASS_H_ */
