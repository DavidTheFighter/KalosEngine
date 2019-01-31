#ifndef RENDERERCORE_RENDERER_COMMON_H_
#define RENDERERCORE_RENDERER_COMMON_H_

#include <common.h>

#define HLSL_MAX_SAMPLER_COUNT 16
#define HLSL_MAX_CONSTANT_BUFFER_COUNT 16
#define HLSL_MAX_INPUT_ATTACHMENT_COUNT 8

#define ROOT_CONSTANT_REGISTER_SPACE 42

#ifdef _WIN32
#define BUILD_D3D12_BACKEND 1
#else
#define BUILD_D3D12_BACKEND 0
#endif

#define BUILD_VULKAN_BACKEND 1

#define RENDER_DEBUG_MARKERS 1
#define D3D12_DEBUG_COMPATIBILITY_CHECKS 1
#define VULKAN_DEBUG_COMPATIBILITY_CHECKS 1


#endif /* RENDERERCORE_RENDERER_COMMON_H_ */