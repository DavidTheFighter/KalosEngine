/*
 * VulkanFormats.h
 *
 *  Created on: Sep 15, 2017
 *      Author: david
 */

#ifndef RENDERING_VULKAN_VULKANENUMS_H_
#define RENDERING_VULKAN_VULKANENUMS_H_
#if BUILD_VULKAN_BACKEND

inline VkAccessFlags getAccessFlagsForImageLayoutTransition(VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_GENERAL:
			return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_ACCESS_SHADER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_ACCESS_HOST_WRITE_BIT;
		case VK_IMAGE_LAYOUT_UNDEFINED:
		default:
			return 0;
	}
}

inline VkAccessFlags getAccessFlagsForBufferLayoutTransition(BufferLayout layout)
{
	switch (layout)
	{
		case BUFFER_LAYOUT_VERTEX_BUFFER:
			return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
		case BUFFER_LAYOUT_INDEX_BUFFER:
			return VK_ACCESS_INDEX_READ_BIT;
		case BUFFER_LAYOUT_CONSTANT_BUFFER:
			return VK_ACCESS_UNIFORM_READ_BIT;
		case BUFFER_LAYOUT_VERTEX_CONSTANT_BUFFER:
			return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;
		case BUFFER_LAYOUT_VERTEX_INDEX_BUFFER:
			return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;
		case BUFFER_LAYOUT_VERTEX_INDEX_CONSTANT_BUFFER:
			return VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;
		case BUFFER_LAYOUT_GENERAL:
			return VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
		case BUFFER_LAYOUT_INDIRECT_BUFFER:
			return VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
		case BUFFER_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		default:
			return 0;
	}
}

#define VKX_PIPELINE_SHADER_STAGE_ALL_SHADERS VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT | VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT | VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT

inline VkPipelineStageFlags getPipelineStagesForImageLayout(VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_GENERAL:
			return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // TODO See if all cmds is necessary
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
			return VKX_PIPELINE_SHADER_STAGE_ALL_SHADERS;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VKX_PIPELINE_SHADER_STAGE_ALL_SHADERS;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_PIPELINE_STAGE_HOST_BIT;
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		default:
			return 0;
	}
}

inline VkPipelineStageFlags getPipelineStagesForBufferLayout(BufferLayout layout)
{
	switch (layout)
	{
		case BUFFER_LAYOUT_VERTEX_BUFFER:
			return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		case BUFFER_LAYOUT_INDEX_BUFFER:
			return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		case BUFFER_LAYOUT_CONSTANT_BUFFER:
			return VKX_PIPELINE_SHADER_STAGE_ALL_SHADERS;
		case BUFFER_LAYOUT_VERTEX_CONSTANT_BUFFER:
			return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VKX_PIPELINE_SHADER_STAGE_ALL_SHADERS;
		case BUFFER_LAYOUT_VERTEX_INDEX_BUFFER:
			return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
		case BUFFER_LAYOUT_VERTEX_INDEX_CONSTANT_BUFFER:
			return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VKX_PIPELINE_SHADER_STAGE_ALL_SHADERS;
		case BUFFER_LAYOUT_GENERAL:
			return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; // TODO See if all cmds is necessary
		case BUFFER_LAYOUT_INDIRECT_BUFFER:
			return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
		case BUFFER_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case BUFFER_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		default:
			return 0;
	}
}

inline VkCullModeFlags PolygonCullModetoVkCullModeFlags(PolygonCullMode cullMode)
{
	switch (cullMode)
	{
		case POLYGON_CULL_MODE_FRONT:
			return VK_CULL_MODE_FRONT_BIT;
		case POLYGON_CULL_MODE_BACK:
			return VK_CULL_MODE_BACK_BIT;
		case POLYGON_CULL_MODE_NONE:
		default:
			return VK_CULL_MODE_NONE;
	}
}

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

#endif
#endif /* RENDERING_VULKAN_VULKANENUMS_H_ */
