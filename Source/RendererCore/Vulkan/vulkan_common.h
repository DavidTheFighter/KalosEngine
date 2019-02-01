
#ifndef RENDERING_VULKAN_VULKAN_COMMON_H_
#define RENDERING_VULKAN_VULKAN_COMMON_H_

#include <RendererCore/renderer_common.h>
#if BUILD_VULKAN_BACKEND

#define VENDOR_ID_AMD 		0x1002
#define VENDOR_ID_NVIDIA 	0x10DE
#define VENDOR_ID_INTEL 	0x8086
#define VENDOR_ID_ARM 		0x13B5
#define VENDOR_ID_QUALCOMM 	0x5143
#define VENDOR_ID_IMGTEC 	0x1010

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#define HLSL_SPV_SAMPLER_OFFSET (0)
#define HLSL_SPV_CONSTANT_BUFFER_OFFSET (HLSL_SPV_SAMPLER_OFFSET + HLSL_MAX_SAMPLER_COUNT)
#define HLSL_SPV_INPUT_ATTACHMENT_OFFSET (HLSL_SPV_CONSTANT_BUFFER_OFFSET + HLSL_MAX_CONSTANT_BUFFER_COUNT)
#define HLSL_SPV_STORAGE_BUFFER_OFFSET (HLSL_SPV_INPUT_ATTACHMENT_OFFSET + HLSL_MAX_INPUT_ATTACHMENT_COUNT)
#define HLSL_SPV_STORAGE_TEXTURE_OFFSET (HLSL_SPV_STORAGE_BUFFER_OFFSET + HLSL_MAX_STORAGE_BUFFER_COUNT)
#define HLSL_SPV_TEXTURE_OFFSET (HLSL_SPV_STORAGE_TEXTURE_OFFSET + HLSL_MAX_STORAGE_TEXTURE_COUNT)

#define VK_HLSL_PUSHCONSTANTBUFFER_PREPROCESSOR "\"PushConstantBuffer\"=\"[[vk::push_constant]] cbuffer __VK_PushConstantsUniformBuffer_" STRINGIZE(ROOT_CONSTANT_REGISTER_SPACE) " : register(b0, space" STRINGIZE(ROOT_CONSTANT_REGISTER_SPACE) ")\""

#define VK_HLSL_SAMPLER_PREPROCESSOR "\"SamplerBinding(TYPE, NAME, BINDING, SET)\"=\"[[vk::binding(BINDING + " STRINGIZE(HLSL_SPV_SAMPLER_OFFSET) ", SET)]] TYPE NAME\""
#define VK_HLSL_CBUFFER_PREPROCESSOR "\"CBufferBinding(TYPE, NAME, BINDING, SET)\"=\"[[vk::binding(BINDING + " STRINGIZE(HLSL_SPV_CONSTANT_BUFFER_OFFSET) ", SET)]] TYPE NAME\""
#define VK_HLSL_INPUT_ATTACHMENT_PREPROCESSOR "\"InputAttachmentBinding(TYPE, NAME, INDEX, SET)\"=\"[[vk::input_attachment_index(INDEX)]] [[vk::binding(INDEX + " STRINGIZE(HLSL_SPV_INPUT_ATTACHMENT_OFFSET) ", SET)]] TYPE NAME\""
#define VK_HLSL_STORAGE_BUFFER_PREPROCESSOR "\"StorageBufferBinding(TYPE, NAME, INDEX, SET)\"=\"[[vk::binding(BINDING + " STRINGIZE(HLSL_SPV_STORAGE_BUFFER_OFFSET) ", SET)]] TYPE NAME\""
#define VK_HLSL_STORAGE_TEXTURE_PREPROCESSOR "\"StorageTextureBinding(TYPE, NAME, BINDING, SET)\"=\"[[vk::binding(BINDING + " STRINGIZE(HLSL_SPV_STORAGE_TEXTURE_OFFSET) ", SET)]] TYPE NAME\""
#define VK_HLSL_TEXTURE_PREPROCESSOR "\"TextureBinding(TYPE, NAME, BINDING, SET)\"=\"[[vk::binding(BINDING + " STRINGIZE(HLSL_SPV_TEXTURE_OFFSET) ", SET)]] TYPE NAME\""

class VulkanExtensions
{
	public:

		static bool enabled_VK_EXT_debug_marker;
		static bool enabled_VK_AMD_rasterization_order;

		static PFN_vkDebugMarkerSetObjectTagEXT DebugMarkerSetObjectTagEXT;
		static PFN_vkDebugMarkerSetObjectNameEXT DebugMarkerSetObjectNameEXT;
		static PFN_vkCmdDebugMarkerBeginEXT CmdDebugMarkerBeginEXT;
		static PFN_vkCmdDebugMarkerEndEXT CmdDebugMarkerEndEXT;
		static PFN_vkCmdDebugMarkerInsertEXT CmdDebugMarkerInsertEXT;

		static void getProcAddresses (VkDevice device);
};

inline VkResult CreateDebugReportCallbackEXT (VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback)
{
	auto func = (PFN_vkCreateDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else
	{
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

inline void DestroyDebugReportCallbackEXT (VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr)
	{
		func(instance, callback, pAllocator);
	}
}

#if RENDER_DEBUG_MARKERS
inline void debugMarkerBeginRegion (VkCommandBuffer cmdBuffer, const std::string &name, const glm::vec4 &color)
{
	VkDebugMarkerMarkerInfoEXT info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
	info.pMarkerName = name.c_str();
	memcpy(info.color, &color.x, sizeof(color));

	vkCmdDebugMarkerBeginEXT(cmdBuffer, &info);
}

inline void debugMarkerEndRegion (VkCommandBuffer cmdBuffer)
{
	vkCmdDebugMarkerEndEXT(cmdBuffer);
}

template<typename VulkanObject>
inline void debugMarkerSetName (VkDevice device, VulkanObject obj, VkDebugReportObjectTypeEXT objType, const std::string &name)
{
	VkDebugMarkerObjectNameInfoEXT nameInfo = {};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = objType;
	nameInfo.object = (uint64_t) obj;
	nameInfo.pObjectName = name.c_str();

	vkDebugMarkerSetObjectNameEXT(device, &nameInfo);
}
#else
inline void debugMarkerBeginRegion (VkCommandBuffer cmdBuffer, const std::string &name, const glm::vec4 &color)
{}
inline void debugMarkerEndRegion (VkCommandBuffer cmdBuffer)
{}
template<typename VulkanObject>
inline void debugMarkerSetName (VkDevice device, VulkanObject obj, VkDebugReportObjectTypeEXT objType, const std::string &name)
{}

#endif

inline std::string getVkResultString (VkResult result)
{
	switch (result)
	{
		case VK_SUCCESS:
			return "VK_SUCCESS";
		case VK_NOT_READY:
			return "VK_NOT_READY";
		case VK_TIMEOUT:
			return "VK_TIMEOUT";
		case VK_EVENT_SET:
			return "VK_EVENT_SET";
		case VK_EVENT_RESET:
			return "VK_EVENT_RESET";
		case VK_INCOMPLETE:
			return "VK_INCOMPLETE";
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "VK_ERROR_OUT_OF_HOST_MEMORY";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
		case VK_ERROR_INITIALIZATION_FAILED:
			return "VK_ERROR_INITIALIZATION_FAILED";
		case VK_ERROR_DEVICE_LOST:
			return "VK_ERROR_DEVICE_LOST";
		case VK_ERROR_MEMORY_MAP_FAILED:
			return "VK_ERROR_MEMORY_MAP_FAILED";
		case VK_ERROR_LAYER_NOT_PRESENT:
			return "VK_ERROR_LAYER_NOT_PRESENT";
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "VK_ERROR_EXTENSION_NOT_PRESENT";
		case VK_ERROR_FEATURE_NOT_PRESENT:
			return "VK_ERROR_FEATURE_NOT_PRESENT";
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "VK_ERROR_INCOMPATIBLE_DRIVER";
		case VK_ERROR_TOO_MANY_OBJECTS:
			return "VK_ERROR_TOO_MANY_OBJECTS";
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			return "VK_ERROR_FORMAT_NOT_SUPPORTED";
		case VK_ERROR_FRAGMENTED_POOL:
			return "VK_ERROR_FRAGMENTED_POOL";
		case VK_ERROR_SURFACE_LOST_KHR:
			return "VK_ERROR_SURFACE_LOST_KHR";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
		case VK_SUBOPTIMAL_KHR:
			return "VK_SUBOPTIMAL_KHR";
		case VK_ERROR_OUT_OF_DATE_KHR:
			return "VK_ERROR_OUT_OF_DATE_KHR";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
			return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
		case VK_ERROR_VALIDATION_FAILED_EXT:
			return "VK_ERROR_VALIDATION_FAILED_EXT";
		case VK_ERROR_INVALID_SHADER_NV:
			return "VK_ERROR_INVALID_SHADER_NV";
		case VK_ERROR_OUT_OF_POOL_MEMORY_KHR:
			return "VK_ERROR_OUT_OF_POOL_MEMORY_KHR";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR:
			return "VK_ERROR_INVALID_EXTERNAL_HANDLE_KHR";
		default:
			return "Dunno" + toString(result);
	}
}

inline std::string getVkVendorString (uint32_t vendor)
{
	switch (vendor)
	{
		case VENDOR_ID_AMD:
			return "AMD";
		case VENDOR_ID_NVIDIA:
			return "NVIDIA";
		case VENDOR_ID_INTEL:
			return "Intel";
		case VENDOR_ID_ARM:
			return "ARM";
		case VENDOR_ID_QUALCOMM:
			return "Qualcomm";
		case VENDOR_ID_IMGTEC:
			return "ImgTec";
		default:
			return toString(vendor);
	}
}

#define VK_CHECK_RESULT(f)		\
{								\
	VkResult res = (f);			\
	if (res != VK_SUCCESS) {		\
		Log::get()->error("Vulkan Error: VkResult is \"{}\" in file \"{}\", at line {}", getVkResultString(res).c_str(), __FILE__, __LINE__); \
		system("pause");/*throw std::runtime_error("vulkan error");*/	\
	}								\
}									\

#endif
#endif /* RENDERING_VULKAN_VULKAN_COMMON_H_ */
