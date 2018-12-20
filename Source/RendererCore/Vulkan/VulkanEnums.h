/*
 * VulkanFormats.h
 *
 *  Created on: Sep 15, 2017
 *      Author: david
 */

#ifndef RENDERING_VULKAN_VULKANENUMS_H_
#define RENDERING_VULKAN_VULKANENUMS_H_

inline VkCommandPoolCreateFlags toVkCommandPoolCreateFlags (CommandPoolFlags flags)
{
	// Generic command pool flags map directly to vulkan command pool create flags
	return static_cast<VkCommandPoolCreateFlags>(flags);
}

inline VkCommandBufferLevel toVkCommandBufferLevel (CommandBufferLevel level)
{
	// Generic command buffer levels map directly to vulkan command buffer levels
	return static_cast<VkCommandBufferLevel>(level);
}

inline VkCommandBufferUsageFlags toVkCommandBufferUsageFlags (CommandBufferUsageFlags usage)
{
	// Generic command buffer usages map directly to vulkan command buffer usages
	return static_cast<VkCommandBufferUsageFlags> (usage);
}

inline VkSubpassContents toVkSubpassContents (SubpassContents contents)
{
	// Generic subpass contents map directly to vulkan subpass contents
	return static_cast<VkSubpassContents> (contents);
}

inline VkPipelineBindPoint toVkPipelineBindPoint (PipelineBindPoint point)
{
	// Generic pipeline bind points map directly to vulkan pipeline bind points
	return static_cast<VkPipelineBindPoint> (point);
}

inline VkShaderStageFlags toVkShaderStageFlags (ShaderStageFlags flags)
{
	// Generic shader stage flags map directly to vulkan shader stage flags
	return static_cast<VkShaderStageFlags> (flags);
}

inline VkShaderStageFlagBits toVkShaderStageFlagBits (ShaderStageFlagBits bits)
{
	//Generic shader stage flag bits map directly to vulkan shader stage flag bits
	return static_cast<VkShaderStageFlagBits> (bits);
}

inline VkVertexInputRate toVkVertexInputRate (VertexInputRate rate)
{
	// Generic input rates map directly to vulkan input rates
	return static_cast<VkVertexInputRate> (rate);
}

inline VkPrimitiveTopology toVkPrimitiveTopology (PrimitiveTopology topology)
{
	// Generic primite topologies map directly to vulkan primitive topologies
	return static_cast<VkPrimitiveTopology> (topology);
}

inline VkColorComponentFlags toVkColorComponentFlags (ColorComponentFlags flags)
{
	// Generic color component flags map directly to vulkan color component flags
	return static_cast<VkColorComponentFlags> (flags);
}

inline VkDynamicState toVkDynamicState (DynamicState state)
{
	// Generic dynamic states map directly to vulkan dynamic states
	return static_cast<VkDynamicState> (state);
}

inline VkPolygonMode toVkPolygonMode (PolygonMode mode)
{
	// Generic polygon modes map directly to vulkan polygon modes
	return static_cast<VkPolygonMode> (mode);
}

inline VkBlendFactor toVkBlendFactor (BlendFactor factor)
{
	// Generic blend factors map directly to vulkan blend factors
	return static_cast<VkBlendFactor> (factor);
}

inline VkBlendOp toVkBlendOp (BlendOp op)
{
	// Generic blend ops map directly to vulkan blend ops
	return static_cast<VkBlendOp> (op);
}

inline VkCompareOp toVkCompareOp (CompareOp op)
{
	//  Generic compare ops map direclty to vulkan compare ops
	return static_cast<VkCompareOp> (op);
}

inline VkLogicOp toVkLogicOp (LogicOp op)
{
	// Generic logic ops map directly to vulkan logic ops
	return static_cast<VkLogicOp> (op);
}

inline VkDescriptorType toVkDescriptorType (DescriptorType type)
{
	// Generic descriptor types map directly to vulkan descriptor types
	return static_cast<VkDescriptorType> (type);
}

inline VkAccessFlags toVkAccessFlags (AccessFlags flags)
{
	// Generic access flags map directly to vulkan access flags
	return static_cast<VkAccessFlags> (flags);
}

inline VkPipelineStageFlags toVkPipelineStageFlags (PipelineStageFlags flags)
{
	// Generic pipeline stage flags map directly to vulkan pipeline stage flags
	return static_cast<VkPipelineStageFlags> (flags);
}

inline VkPipelineStageFlagBits toVkPipelineStageFlagBits (PipelineStageFlagBits bit)
{
	// Generic pipelien stage flag bits map directly to vulkan pipeline stage flag bits
	return static_cast<VkPipelineStageFlagBits> (bit);
}

inline VkAttachmentLoadOp toVkAttachmentLoadOp (AttachmentLoadOp op)
{
	// Generic load ops map directly to vulkan load ops
	return static_cast<VkAttachmentLoadOp> (op);
}

inline VkAttachmentStoreOp toVkAttachmentStoreOp (AttachmentStoreOp op)
{
	// Generic store ops map directly to vulkan store ops
	return static_cast<VkAttachmentStoreOp> (op);
}

inline VkImageLayout toVkImageLayout (TextureLayout layout)
{
	// Generic image layouts map directly to vulkan image layouts
	return static_cast<VkImageLayout> (layout);
}

inline VkImageUsageFlags toVkImageUsageFlags(TextureUsageFlags flags)
{
	// Generic texture usage flags map directly to vulkan image usage flags
	return static_cast<VkImageUsageFlags> (flags);
}

inline VkFormat toVkFormat (ResourceFormat format)
{
	// Generic formats map directly to vulkan formats
	return static_cast<VkFormat>(format);
}

inline VkImageType toVkImageType (TextureType type)
{
	// Generic image types map directly to vulkan image types
	return static_cast<VkImageType>(type);
}

inline VkImageViewType toVkImageViewType (TextureViewType type)
{
	// Generic image view types map directly to vulkan image view types
	return static_cast<VkImageViewType>(type);
}

inline VkFilter toVkFilter (SamplerFilter filter)
{
	// Generic sampler filters map directly to vulkan filters
	return static_cast<VkFilter>(filter);
}

inline VkSamplerAddressMode toVkSamplerAddressMode (SamplerAddressMode mode)
{
	// Generic sampler address modes map directly to vulkan sampler address modes
	return static_cast<VkSamplerAddressMode>(mode);
}

inline VkSamplerMipmapMode toVkSamplerMipmapMode (SamplerMipmapMode mode)
{
	// Generic sampler mipmap mods map directly to vulkan sampler mipmap modes
	return static_cast<VkSamplerMipmapMode>(mode);
}

inline VkExtent3D toVkExtent (suvec3 vec)
{
	return {vec.x, vec.y, vec.z};
}

inline VmaMemoryUsage toVmaMemoryUsage (MemoryUsage usage)
{
	// Generic memory usage maps directly to vma usages
	return static_cast<VmaMemoryUsage>(usage);
}

inline VkDebugReportObjectTypeEXT toVkDebugReportObjectTypeEXT (RendererObjectType type)
{
	// Map directly blah blah blah
	return static_cast<VkDebugReportObjectTypeEXT> (type);
}

inline bool isVkDepthFormat (VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_D16_UNORM:
		case VK_FORMAT_X8_D24_UNORM_PACK32:
		case VK_FORMAT_D32_SFLOAT:
		case VK_FORMAT_D16_UNORM_S8_UINT:
		case VK_FORMAT_D24_UNORM_S8_UINT:
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return true;
		default:
			return false;
	}
}

#endif /* RENDERING_VULKAN_VULKANENUMS_H_ */
