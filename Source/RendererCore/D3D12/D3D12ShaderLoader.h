#ifndef RENDERERCORE_D3D12_D3D12SHADERLOADER_H_
#define RENDERERCORE_D3D12_D3D12SHADERLOADER_H_

#include <RendererCore/D3D12/d3d12_common.h>
#if BUILD_D3D12_BACKEND

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class D3D12ShaderLoader
{
public:

	static ShaderModule loadShaderModuleFromSource(const std::string &source, const std::string &sourceName, ShaderStageFlagBits stage, ShaderSourceLanguage lang, const std::string &entryPoint);

private:

	static std::vector<char> parseHLSLSource(const std::vector<char> &source);
};

#endif
#endif /* RENDERERCORE_D3D12_D3D12SHADERLOADER_H_*/
