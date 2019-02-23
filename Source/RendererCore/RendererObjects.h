#ifndef RENDERING_RENDERER_RENDEREROBJECTS_H_
#define RENDERING_RENDERER_RENDEREROBJECTS_H_

#include <common.h>

typedef struct RendererTexture
{
		uint32_t width, height, depth;
		ResourceFormat textureFormat;
		uint32_t layerCount;
		uint32_t mipCount;
} RendererTexture;

typedef struct RendererTextureView
{
	RendererTexture *parentTexture;
	ResourceFormat viewFormat;
	TextureViewType viewType;
	uint32_t baseMip;
	uint32_t mipCount;
	uint32_t baseLayer;
	uint32_t layerCount;
} RendererTextureView;

typedef struct RendererSampler
{
} RendererSampler;

typedef struct TextureSubresourceRange
{
		uint32_t baseMipLevel;
		uint32_t levelCount;
		uint32_t baseArrayLayer;
		uint32_t layerCount;
} TextureSubresourceRange;

typedef struct TextureSubresourceLayers
{
	    uint32_t              mipLevel;
	    uint32_t              baseArrayLayer;
	    uint32_t              layerCount;
} TextureSubresourceLayers;

typedef struct RendererStagingBuffer
{
	size_t bufferSize;
} RendererStagingBuffer;

typedef struct RendererStagingTexture
{
	size_t bufferSize;
	ResourceFormat textureFormat;
	uint32_t width;
	uint32_t height;
	uint32_t depth;
	uint32_t mipLevels;
	uint32_t arrayLayers;

} RendererStagingTexture;

typedef struct RendererBuffer
{
	size_t bufferSize;
	BufferUsageFlags usage;

} RendererBuffer;

// Note at least for now I'm disregarding stencil operations
typedef struct RendererAttachmentDescription
{
		ResourceFormat format;
		AttachmentLoadOp loadOp;
		AttachmentStoreOp storeOp;
		TextureLayout initialLayout;
		TextureLayout finalLayout;
} AttachmentDescription;

typedef struct RendererAttachmentReference
{
		uint32_t attachment;
		TextureLayout layout;
} AttachmentReference;

typedef struct RendererSupassDependency
{
		uint32_t srcSubpass;
		uint32_t dstSubpass;
		PipelineStageFlags srcStageMask;
		PipelineStageFlags dstStageMask;
		AccessFlags srcAccessMask;
		AccessFlags dstAccessMask;
		bool byRegionDependency;
} SubpassDependency;

typedef struct RendererSubpassDescription
{
		PipelineBindPoint bindPoint;
		std::vector<AttachmentReference> colorAttachments;
		std::vector<AttachmentReference> inputAttachments;
		bool hasDepthAttachment;
		AttachmentReference depthStencilAttachment;
		std::vector<uint32_t> preserveAttachments;
} SubpassDescription;

typedef struct RendererRenderPass
{
		std::vector<AttachmentDescription> attachments;
		std::vector<SubpassDescription> subpasses;
		std::vector<SubpassDependency> subpassDependencies;
} RendererRenderPass;

typedef struct RendererDescriptorStaticSampler
{
	uint32_t samplerBinding;
	SamplerAddressMode addressMode = SAMPLER_ADDRESS_MODE_REPEAT;
	SamplerFilter minFilter = SAMPLER_FILTER_LINEAR;
	SamplerFilter magFilter = SAMPLER_FILTER_LINEAR;
	float maxAnisotropy = 1.0f;
	float minLod = 0.0f;
	float maxLod = 0.0f;
	float mipLodBias = 0.0f;
	SamplerMipmapMode mipmapMode = SAMPLER_MIPMAP_MODE_LINEAR;

	bool operator==(const RendererDescriptorStaticSampler &other)
	{
		if (samplerBinding != other.samplerBinding)
			return false;
		if (addressMode != other.addressMode)
			return false;
		if (minFilter != other.minFilter)
			return false;
		if (magFilter != other.magFilter)
			return false;
		if (maxAnisotropy != other.maxAnisotropy)
			return false;
		if (minLod != other.minLod)
			return false;
		if (maxLod != other.maxLod)
			return false;
		if (mipLodBias != other.mipLodBias)
			return false;
		if (mipmapMode != other.mipmapMode)
			return false;

		return true;
	}

	bool operator!=(const RendererDescriptorStaticSampler &other)
	{
		return !(operator==(other));
	}

} DescriptorStaticSampler;

typedef struct RendererDescriptorSetLayoutDescription
{
	uint32_t samplerDescriptorCount;
	uint32_t constantBufferDescriptorCount;
	uint32_t inputAttachmentDescriptorCount;
	uint32_t textureDescriptorCount;
	uint32_t storageBufferDescriptorCount;
	uint32_t storageTextureDescriptorCount;

	std::vector<ShaderStageFlags> samplerBindingsShaderStageAccess;
	std::vector<ShaderStageFlags> constantBufferBindingsShaderStageAccess;
	std::vector<ShaderStageFlags> inputAttachmentBindingsShaderStageAccess;
	std::vector<ShaderStageFlags> textureBindingsShaderStageAccess;
	std::vector<ShaderStageFlags> storageBufferBindingsShaderStageAccess;
	std::vector<ShaderStageFlags> storageTextureBindingsShaderStageAccess;

	std::vector<DescriptorStaticSampler> staticSamplers;

	bool operator==(const RendererDescriptorSetLayoutDescription &other)
	{
		if (samplerDescriptorCount != other.samplerDescriptorCount || constantBufferDescriptorCount != other.constantBufferDescriptorCount ||
			inputAttachmentDescriptorCount != other.inputAttachmentDescriptorCount || textureDescriptorCount != other.textureDescriptorCount ||
			storageBufferDescriptorCount != other.storageBufferDescriptorCount || storageTextureDescriptorCount != other.storageTextureDescriptorCount)
		{
			return false;
		}

		if (samplerBindingsShaderStageAccess.size() != other.samplerBindingsShaderStageAccess.size())
			return false;

		if (constantBufferBindingsShaderStageAccess.size() != other.constantBufferBindingsShaderStageAccess.size())
			return false;

		if (inputAttachmentBindingsShaderStageAccess.size() != other.inputAttachmentBindingsShaderStageAccess.size())
			return false;

		if (textureBindingsShaderStageAccess.size() != other.textureBindingsShaderStageAccess.size())
			return false;

		if (storageBufferBindingsShaderStageAccess.size() != other.storageBufferBindingsShaderStageAccess.size())
			return false;

		if (storageTextureBindingsShaderStageAccess.size() != other.storageTextureBindingsShaderStageAccess.size())
			return false;

		if (staticSamplers.size() != other.staticSamplers.size())
			return false;

		for (size_t i = 0; i < samplerBindingsShaderStageAccess.size(); i++)
			if (samplerBindingsShaderStageAccess[i] != other.samplerBindingsShaderStageAccess[i])
				return false;

		for (size_t i = 0; i < constantBufferBindingsShaderStageAccess.size(); i++)
			if (constantBufferBindingsShaderStageAccess[i] != other.constantBufferBindingsShaderStageAccess[i])
				return false;

		for (size_t i = 0; i < inputAttachmentBindingsShaderStageAccess.size(); i++)
			if (inputAttachmentBindingsShaderStageAccess[i] != other.inputAttachmentBindingsShaderStageAccess[i])
				return false;

		for (size_t i = 0; i < textureBindingsShaderStageAccess.size(); i++)
			if (textureBindingsShaderStageAccess[i] != other.textureBindingsShaderStageAccess[i])
				return false;

		for (size_t i = 0; i < storageBufferBindingsShaderStageAccess.size(); i++)
			if (storageBufferBindingsShaderStageAccess[i] != other.storageBufferBindingsShaderStageAccess[i])
				return false;

		for (size_t i = 0; i < storageTextureBindingsShaderStageAccess.size(); i++)
			if (storageTextureBindingsShaderStageAccess[i] != other.storageTextureBindingsShaderStageAccess[i])
				return false;
		
		for (size_t i = 0; i < staticSamplers.size(); i++)
			if (staticSamplers[i] != other.staticSamplers[i])
				return false;

		return true;
	}

	bool operator!=(const RendererDescriptorSetLayoutDescription &other)
	{
		return !(operator!=(other));
	}

} DescriptorSetLayoutDescription;

typedef struct RendererShaderModule
{
		ShaderStageFlagBits stage;
		std::string entryPoint;

} RendererShaderModule;

typedef struct RendererViewport
{
		float x;
		float y;
		float width;
		float height;
		float minDepth;
		float maxDepth;
} Viewport;

typedef struct RendererScissor
{
		int32_t x;
		int32_t y;
		uint32_t width;
		uint32_t height;
} Scissor;

//f//

typedef struct RendererPushConstantRange
{
		size_t size;
		ShaderStageFlags stageAccessFlags;
} PushConstantRange;

typedef struct RendererPipelineShaderStage
{
		RendererShaderModule *shaderModule;
		// Also eventually specialization constants
} PipelineShaderStage;

typedef struct RendererVertexInputBinding
{
		uint32_t binding;
		uint32_t stride;
		VertexInputRate inputRate;
} VertexInputBinding;

typedef struct RendererVertexInputAttrib
{
		uint32_t binding;
		uint32_t location;
		ResourceFormat format;
		uint32_t offset;
} VertexInputAttribute;

typedef struct RendererPipelineVertexInputInfo
{
		std::vector<VertexInputBinding> vertexInputBindings;
		std::vector<VertexInputAttribute> vertexInputAttribs;
} PipelineVertexInputInfo;

typedef struct RendererPipelineInputAssemblyInfo
{
		PrimitiveTopology topology;
		bool primitiveRestart;
} PipelineInputAssemblyInfo;

typedef struct RendererPipelineTessellationInfo
{
		uint32_t patchControlPoints;
} PipelineTessellationInfo;

typedef struct RendererPipelineRasterizationInfo
{
		bool clockwiseFrontFace;
		bool enableOutOfOrderRasterization;
		PolygonMode polygonMode;
		PolygonCullMode cullMode;
		uint32_t multiSampleCount;
		// Also some depth bias stuff I might implement later

} PipelineRasterizationInfo;

typedef struct RendererPipelineDepthStencilInfo
{
		bool enableDepthTest;
		bool enableDepthWrite;
		CompareOp depthCompareOp;

} PipelineDepthStencilInfo;

typedef struct RendererPipelineColorBlendAttachment
{
		bool blendEnable;
		BlendFactor srcColorBlendFactor;
		BlendFactor dstColorBlendFactor;
		BlendOp colorBlendOp;
		BlendFactor srcAlphaBlendFactor;
		BlendFactor dstAlphaBlendFactor;
		BlendOp alphaBlendOp;
		ColorComponentFlags colorWriteMask;
} PipelineColorBlendAttachment;

typedef struct RendererPipelineColorBlendInfo
{
		bool logicOpEnable;
		LogicOp logicOp;
		std::vector<PipelineColorBlendAttachment> attachments;
		float blendConstants[4];

} PipelineColorBlendInfo;

typedef struct RendererGraphicsPipelineInfo
{
		std::vector<PipelineShaderStage> stages;
		PipelineVertexInputInfo vertexInputInfo;
		PipelineInputAssemblyInfo inputAssemblyInfo;
		PipelineRasterizationInfo rasterizationInfo;
		PipelineDepthStencilInfo depthStencilInfo;
		PipelineColorBlendInfo colorBlendInfo;

		uint32_t tessellationPatchControlPoints; // Only used if there is a tessellation shader in 'stages'

		PushConstantRange inputPushConstants;
		std::vector<DescriptorSetLayoutDescription> inputSetLayouts;

		//PipelineInputLayout inputLayout;
		//RendererRenderPass *renderPass;
		//uint32_t subpass;

} GraphicsPipelineInfo;

typedef struct RendererComputePipelineInfo
{
	PipelineShaderStage shader;

	PushConstantRange inputPushConstants;
	std::vector<DescriptorSetLayoutDescription> inputSetLayouts;

} ComputePipelineInfo;

typedef struct RendererDescriptorSet
{

} RendererDescriptorSet;

typedef struct RendererPipeline
{
	PipelineBindPoint pipelineBindPoint;

	GraphicsPipelineInfo gfxPipelineInfo;
	ComputePipelineInfo computePipelineInfo;
} RendererPipeline;

typedef struct RendererFramebuffer
{

} RendererFramebuffer;

typedef struct RendererDescriptorSamplerInfo
{
	RendererSampler *sampler;
} DescriptorSamplerInfo;

typedef struct RendererDescriptorTextureInfo
{
		RendererTextureView *view;
		TextureLayout layout;

} DescriptorTextureInfo;

typedef struct RendererDescriptorBufferInfo
{
		RendererBuffer *buffer;
		size_t offset;
		size_t range;
} DescriptorBufferInfo;

typedef struct RendererDescriptorWriteInfo
{
		uint32_t dstBinding;
		DescriptorType descriptorType;

		union
		{
			RendererDescriptorSamplerInfo samplerInfo;
			DescriptorTextureInfo inputAttachmentInfo;
			RendererDescriptorBufferInfo bufferInfo;
			DescriptorTextureInfo sampledTextureInfo;
		};

} DescriptorWriteInfo;

typedef union VRendererClearColorValue {
    float       float32[4];
    int32_t     int32[4];
    uint32_t    uint32[4];
} ClearColorValue;

typedef struct RendererClearDepthStencilValue {
    float       depth;
    uint32_t    stencil;
} ClearDepthStencilValue;

typedef union RendererClearValue {
    ClearColorValue           color;
    ClearDepthStencilValue    depthStencil;
} ClearValue;

typedef struct TextureBlitInfo
{
		TextureSubresourceLayers srcSubresource;
		sivec3 srcOffsets[2];
		TextureSubresourceLayers dstSubresource;
		sivec3 dstOffsets[2];
} TextureBlitInfo;

//f//

typedef struct RendererFence
{
} RendererFence;

typedef struct RendererSemaphore
{

} RendererSemaphore;

typedef struct RendererTextureTransitionBarrier
{
	RendererTexture *texture;
	TextureLayout oldLayout;
	TextureLayout newLayout;
	TextureSubresourceRange subresourceRange;

} TextureTransitionBarrier;

typedef struct RendererBufferTransitionBarrier
{
	RendererBuffer *buffer;
	BufferLayout oldLayout;
	BufferLayout newLayout;

} BufferTransitionBarrier;

typedef struct RendererResourceBarrier
{
	ResourceBarrierType barrierType;

	union
	{
		TextureTransitionBarrier textureTransition;
		BufferTransitionBarrier bufferTransition;
	};
} ResourceBarrier;

typedef RendererRenderPass *RenderPass;
typedef RendererPipeline *Pipeline;
//typedef RendererDescriptorSetLayout *DescriptorSetLayout;
typedef RendererDescriptorSet *DescriptorSet;
typedef RendererShaderModule *ShaderModule;
typedef RendererFramebuffer *Framebuffer;
typedef RendererTexture *Texture;
typedef RendererTextureView *TextureView;
typedef RendererSampler *Sampler;
typedef RendererBuffer *Buffer;
//typedef RendererCommandPool *CommandPool;
typedef RendererStagingBuffer *StagingBuffer;
typedef RendererStagingTexture *StagingTexture;
typedef RendererFence *Fence;
typedef RendererSemaphore *Semaphore;

#include <RendererCore/RendererCommandPool.h>
#include <RendererCore/RendererCommandBuffer.h>
#include <RendererCore/RendererDescriptorPool.h>
#include <RendererCore/RendererRenderGraph.h>
#include <RendererCore/RenderGraphRenderPass.h>

#endif /* RENDERING_RENDERER_RENDEREROBJECTS_H_ */
