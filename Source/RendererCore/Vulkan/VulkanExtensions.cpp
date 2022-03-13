#include <RendererCore/Vulkan/vulkan_common.h>
#if BUILD_VULKAN_BACKEND

void VulkanExtensions::getProcAddresses (VkDevice device)
{
	if (RENDER_DEBUG_MARKERS && enabled_VK_EXT_debug_marker)
	{
		DebugMarkerSetObjectTagEXT = (PFN_vkDebugMarkerSetObjectTagEXT) vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectTagEXT");
		DebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT) vkGetDeviceProcAddr(device, "vkDebugMarkerSetObjectNameEXT");
		CmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT) vkGetDeviceProcAddr(device, "vkCmdDebugMarkerBeginEXT");
		CmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT) vkGetDeviceProcAddr(device, "vkCmdDebugMarkerEndEXT");
		CmdDebugMarkerInsertEXT = (PFN_vkCmdDebugMarkerInsertEXT) vkGetDeviceProcAddr(device, "vkCmdDebugMarkerInsertEXT");
	}
}

#if RENDER_DEBUG_MARKERS

bool VulkanExtensions::enabled_VK_EXT_debug_marker = false;
bool VulkanExtensions::enabled_VK_AMD_rasterization_order = false;

PFN_vkDebugMarkerSetObjectTagEXT VulkanExtensions::DebugMarkerSetObjectTagEXT = VK_NULL_HANDLE;
PFN_vkDebugMarkerSetObjectNameEXT VulkanExtensions::DebugMarkerSetObjectNameEXT = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerBeginEXT VulkanExtensions::CmdDebugMarkerBeginEXT = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerEndEXT VulkanExtensions::CmdDebugMarkerEndEXT = VK_NULL_HANDLE;
PFN_vkCmdDebugMarkerInsertEXT VulkanExtensions::CmdDebugMarkerInsertEXT = VK_NULL_HANDLE;

VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectTagEXT (VkDevice device, const VkDebugMarkerObjectTagInfoEXT* pTagInfo)
{
	if (!VulkanExtensions::enabled_VK_EXT_debug_marker)
		return VK_SUCCESS;

	return VulkanExtensions::DebugMarkerSetObjectTagEXT(device, pTagInfo);
}

VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectNameEXT (VkDevice device, const VkDebugMarkerObjectNameInfoEXT* pNameInfo)
{
	if (!VulkanExtensions::enabled_VK_EXT_debug_marker)
		return VK_SUCCESS;

	return VulkanExtensions::DebugMarkerSetObjectNameEXT(device, pNameInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerBeginEXT (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
	if (!VulkanExtensions::enabled_VK_EXT_debug_marker)
		return;

	VulkanExtensions::CmdDebugMarkerBeginEXT(commandBuffer, pMarkerInfo);
}

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerEndEXT (VkCommandBuffer commandBuffer)
{
	if (!VulkanExtensions::enabled_VK_EXT_debug_marker)
		return;

	VulkanExtensions::CmdDebugMarkerEndEXT(commandBuffer);
}

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerInsertEXT (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
	if (!VulkanExtensions::enabled_VK_EXT_debug_marker)
		return;

	VulkanExtensions::CmdDebugMarkerInsertEXT(commandBuffer, pMarkerInfo);
}

#else

VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectTagEXT (VkDevice device, const VkDebugMarkerObjectTagInfoEXT* pTagInfo)
{
	return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkDebugMarkerSetObjectNameEXT (VkDevice device, const VkDebugMarkerObjectNameInfoEXT* pNameInfo)
{
	return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerBeginEXT (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
}

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerEndEXT (VkCommandBuffer commandBuffer)
{
}

VKAPI_ATTR void VKAPI_CALL vkCmdDebugMarkerInsertEXT (VkCommandBuffer commandBuffer, const VkDebugMarkerMarkerInfoEXT* pMarkerInfo)
{
}

#endif
#endif
