#ifndef RENDERERCORE_D3D12_D3D12_COMMON_H_
#define RENDERERCORE_D3D12_D3D12_COMMON_H_

#include <RendererCore/renderer_common.h>
#if BUILD_D3D12_BACKEND

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <d3dx12.h>

#include <comdef.h>

#define D3D12_HLSL_PUSHCONSTANTBUFFER_PREPROCESSOR "cbuffer __D3D12_PushConstantsUniformBuffer_" STRINGIZE(ROOT_CONSTANT_REGISTER_SPACE) " : register(b0, space" STRINGIZE(ROOT_CONSTANT_REGISTER_SPACE) ")"

#define DX_CHECK_RESULT(hr)		\
{								\
	if (FAILED(hr)) {		\
		_com_error err(hr);		\
		LPCTSTR errMsg = err.ErrorMessage();	\
		Log::get()->error("D3D12 Error: HRESULT is \"{}\", in file \"{}\", at line {}", errMsg, __FILE__, __LINE__); \
		throw std::runtime_error("d3d12 hresult error");	\
	}								\
}									

#define DX_CHECK_RESULT_ERR_BLOB(hr, blob)		\
{								\
	if (FAILED(hr)) {		\
		_com_error err(hr);		\
		LPCTSTR errMsg = err.ErrorMessage();	\
		Log::get()->error("D3D12 Error: HRESULT is \"{}\", in file \"{}\", at line {}. Error message: \"{}\"", errMsg, __FILE__, __LINE__, std::string(reinterpret_cast<char *>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize())); \
		throw std::runtime_error("d3d12 hresult error");	\
	}								\
}

inline HANDLE createEventHandle()
{
	HANDLE eventHandle;

	eventHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
	DEBUG_ASSERT(eventHandle && "Failed to create an event handle!");

	return eventHandle;
}

#endif /* BUILD_D3D12_BACKEND */
#endif /* RENDERERCORE_D3D12_D3D12_COMMON_H_ */