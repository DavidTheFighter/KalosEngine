#ifndef RENDERERCORE_D3D12_D3D12_COMMON_H_
#define RENDERERCORE_D3D12_D3D12_COMMON_H_

#include <common.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <d3dx12.h>

#include <comdef.h>

#define DX_CHECK_RESULT(hr)		\
{								\
	if (FAILED(hr)) {		\
		_com_error err(hr);		\
		LPCTSTR errMsg = err.ErrorMessage();	\
		Log::get()->error("D3D12 Error: HRESULT is \"{}\", in file \"{}\", at line {}", errMsg, __FILE__, __LINE__); \
		throw std::runtime_error("d3d12 error");	\
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