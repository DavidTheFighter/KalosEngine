#ifndef RENDERERCORE_D3D12_D3D12_COMMON_H_
#define RENDERERCORE_D3D12_D3D12_COMMON_H_

#include <RendererCore/renderer_common.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <d3dx12.h>

#include <comdef.h>

#define D3D12_PUSHCONSTANTBUFFER_PREPROCESSOR_STR "cbuffer __D3D12_PushConstantsUniformBuffer_" STRINGIZE(ROOT_CONSTANT_REGISTER_SPACE) " : register(b0, space" STRINGIZE(ROOT_CONSTANT_REGISTER_SPACE) ")"
#define D3D12_HLSL_MERGE_PREPROCESSOR {"__D3D12_MERGE(a, b)", "a##b"}
#define D3D12_HLSL_SAMPLER_PREPROCESSOR {"Sampler(TYPE, NAME, BINDING, SET)", "TYPE NAME : register(__D3D12_MERGE(s, BINDING), __D3D12_MERGE(space, SET))"}
#define D3D12_HLSL_CBUFFER_PREPROCESSOR {"CBuffer(TYPE, NAME, BINDING, SET)", "TYPE NAME : register(__D3D12_MERGE(b, BINDING), __D3D12_MERGE(space, SET))"}
#define D3D12_HLSL_INPUT_ATTACHMENT_PREPROCESSOR {"InputAttachment(TYPE, NAME, INDEX, SET)", "TYPE NAME : register(__D3D12_MERGE(t, INDEX), __D3D12_MERGE(space, SET))"}
#define D3D12_HLSL_SAMPLED_TEXTURE_PREPROCESSOR {"SampledTexture(TYPE, NAME, BINDING, SET)", "TYPE NAME : register(__D3D12_MERGE(t1, BINDING), __D3D12_MERGE(space, SET))"}

#define DX_CHECK_RESULT(hr)		\
{								\
	if (FAILED(hr)) {		\
		_com_error err(hr);		\
		LPCTSTR errMsg = err.ErrorMessage();	\
		Log::get()->error("D3D12 Error: HRESULT is \"{}\", in file \"{}\", at line {}", errMsg, __FILE__, __LINE__); \
		throw std::runtime_error("d3d12 hresult error");	\
	}								\
}									\

inline HANDLE createEventHandle()
{
	HANDLE eventHandle;

	eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	DEBUG_ASSERT(eventHandle && "Failed to create an event handle!");

	return eventHandle;
}

#endif /* RENDERERCORE_D3D12_D3D12_COMMON_H_ */