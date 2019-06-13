#include <RendererCore/D3D12/D3D12ShaderLoader.h>

#if BUILD_D3D12_BACKEND

#include <RendererCore/D3D12/D3D12Enums.h>
#include <RendererCore/D3D12/D3D12Objects.h>

ShaderModule D3D12ShaderLoader::loadShaderModuleFromSource(const std::string &source, const std::string &sourceName, ShaderStageFlagBits stage, ShaderSourceLanguage lang, const std::string &entryPoint)
{
	D3D12ShaderModule *shaderModule = new D3D12ShaderModule();
	ID3DBlob *blob = nullptr, *errorBuf = nullptr;

	const D3D_SHADER_MACRO macroDefines[] = {
		{"PushConstantBuffer", D3D12_HLSL_PUSHCONSTANTBUFFER_PREPROCESSOR},
		{nullptr, nullptr}
	};

	//std::string modSource = source;
	//modSource = std::string(D3D12_HLSL_PUSHCONSTANTBUFFER_PREPROCESSOR) + "\n" + modSource;
	std::vector<char> modSource = parseHLSLSource(std::vector<char>(source.c_str(), source.c_str() + source.length()));

	HRESULT hr = D3DCompile(modSource.data(), modSource.size(), sourceName.c_str(), macroDefines, nullptr, entryPoint.c_str(), shaderStageFlagBitsToD3D12CompilerTargetStr(stage).c_str(), D3DCOMPILE_DEBUG | D3DCOMPILE_ALL_RESOURCES_BOUND | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &blob, &errorBuf);

	if (FAILED(hr))
	{
		Log::get()->error("D3D12 failed to compile a shader, error msg: {}", std::string((char *)errorBuf->GetBufferPointer()));
		throw std::runtime_error("d3d12 shader compile error");
	}

	shaderModule->shaderBytecode = std::unique_ptr<char>(new char[blob->GetBufferSize()]);
	memcpy(shaderModule->shaderBytecode.get(), blob->GetBufferPointer(), blob->GetBufferSize());

	shaderModule->shaderBytecodeLength = blob->GetBufferSize();
	shaderModule->stage = stage;
	shaderModule->entryPoint = entryPoint;

	blob->Release();

	if (errorBuf != nullptr)
		errorBuf->Release();

	return shaderModule;
}

std::map<std::string, D3D12_DESCRIPTOR_RANGE_TYPE> D3D12ShaderLoader_createHLSLResourceTypesMap();

const std::string resourceBindingSpecifier = "binding";
const std::map<std::string, D3D12_DESCRIPTOR_RANGE_TYPE> hlslResourceTypesMap = D3D12ShaderLoader_createHLSLResourceTypesMap();

std::vector<char> D3D12ShaderLoader::parseHLSLSource(const std::vector<char> &source)
{
	// Make a copy of the source code but without duplicate spaces
	std::string noDuplicateSpaceSource;
	noDuplicateSpaceSource.reserve(source.size());

	for (size_t i = 0; i < source.size(); i++)
		if (source[i] != ' ' || (i < source.size() - 1 && source[i + 1] != ' '))
			noDuplicateSpaceSource.push_back(source[i]);

	std::map<uint32_t, std::map<uint32_t, std::tuple<size_t, D3D12_DESCRIPTOR_RANGE_TYPE, uint32_t> > > resourceRegisterInsertPositions; // Positions in source code to insert " : register(x, spacex)" after all resources haves been counted, mapped first by set then by binding to the position

	for (size_t pos = 0; pos < noDuplicateSpaceSource.size(); pos++)
	{
		if (noDuplicateSpaceSource[pos] == ':' && pos < noDuplicateSpaceSource.size() - resourceBindingSpecifier.size() - 2)
		{
			size_t specifierPos;
			std::string specifierSubStr = noDuplicateSpaceSource.substr(pos + 1, resourceBindingSpecifier.size() + 1);

			if ((specifierPos = specifierSubStr.find(resourceBindingSpecifier)) != std::string::npos)
			{
				// Now that we found a binding specifier, parse what binding and set this resource is assigned to
				size_t numPos = pos + specifierPos;

				// Move to the first parenthesis and then skip all spaces until the first number
				while (++numPos < noDuplicateSpaceSource.size() - 1 && (noDuplicateSpaceSource[numPos] != '('));
				while (++numPos < noDuplicateSpaceSource.size() - 1 && noDuplicateSpaceSource[numPos] == ' ');

				size_t firstNumStart = numPos;

				// Move until we hit either spaces or a comma
				while (++numPos < noDuplicateSpaceSource.size() - 1 && noDuplicateSpaceSource[numPos] != ' ' && noDuplicateSpaceSource[numPos] != ',');

				size_t firstNumEnd = numPos;

				// Skip over spaces until we hit the second number
				while (++numPos < noDuplicateSpaceSource.size() - 1 && noDuplicateSpaceSource[numPos] == ' ');

				size_t secondNumStart = numPos;

				// Move until we hit a space or parenthesis and end the number
				while (++numPos < noDuplicateSpaceSource.size() - 1 && noDuplicateSpaceSource[numPos] != ' ' && noDuplicateSpaceSource[numPos] != ')');

				size_t secondNumEnd = numPos;

				uint32_t bindingNumber = std::stoul(noDuplicateSpaceSource.substr(firstNumStart, firstNumEnd - firstNumStart));
				uint32_t setNumber = std::stoul(noDuplicateSpaceSource.substr(secondNumStart, secondNumEnd - secondNumStart));

				// Now that we have our binding and set, we'll remove the " : binding(#, #)" part because it isn't valid vanilla HLSL
				while (numPos < noDuplicateSpaceSource.size() - 1 && noDuplicateSpaceSource[numPos] != ')')
					numPos++;

				noDuplicateSpaceSource.erase(pos, numPos - pos + 1);

				// Next we try to find if this resource is an array, and if so how big
				size_t arrayPos = pos;
				uint32_t arraySize = 1;

				while (arrayPos > 0 && noDuplicateSpaceSource[--arrayPos] == ' ');

				// If we hit a ']' character it's an array
				if (noDuplicateSpaceSource[arrayPos] == ']')
				{
					arraySize = std::numeric_limits<uint32_t>::max();
					arrayPos--;

					while (arrayPos > 0 && noDuplicateSpaceSource[arrayPos] == ' ')
						arrayPos--;

					if (noDuplicateSpaceSource[arrayPos] != '[')
					{
						if (noDuplicateSpaceSource[arrayPos] == ' ')
							arrayPos--;

						size_t arraySizeEndPos = arrayPos;

						while (arrayPos > 0 && noDuplicateSpaceSource[arrayPos] != ' ' && noDuplicateSpaceSource[arrayPos] != '[')
							arrayPos--;

						size_t arraySizeBeginPos = arrayPos + 1;

						arraySize = std::stoul(noDuplicateSpaceSource.substr(arraySizeBeginPos, arraySizeEndPos - arraySizeBeginPos + 1));
					}
				}

				// To finish the resource binding, we add the "[[vk::binding(#, #)]]" before the resource. We start by iterating back until we've found the resource type then add the attribute
				size_t attribPos = pos;

				// Move back through spaces until we hit the resource variable name
				while (attribPos > 0 && noDuplicateSpaceSource[--attribPos] == ' ');

				// Move back until we hit the space in between the resource type and name or the closing '>' of a template
				while (attribPos > 0 && noDuplicateSpaceSource[--attribPos] != ' ' && noDuplicateSpaceSource[attribPos] != '>');

				if (noDuplicateSpaceSource[attribPos] == '>' || noDuplicateSpaceSource[attribPos - 1] == '>')
				{
					while (attribPos > 0 && noDuplicateSpaceSource[--attribPos] != '<');

					if (noDuplicateSpaceSource[attribPos - 1] == ' ')
						attribPos--;
				}

				size_t resourceTypeEndPos = attribPos;

				while (attribPos > 0 && noDuplicateSpaceSource[--attribPos] != ' ' && noDuplicateSpaceSource[attribPos] != '\n' && noDuplicateSpaceSource[attribPos] != ';');

				size_t resourceTypeBeginPos = attribPos + 1;

				std::string resourceTypeStr = noDuplicateSpaceSource.substr(resourceTypeBeginPos, resourceTypeEndPos - resourceTypeBeginPos);

				if (hlslResourceTypesMap.count(resourceTypeStr) == 0)
				{
					Log::get()->error("D3D12ShaderLoader: Invalid resource type {} in HLSL source!\n", resourceTypeStr);
					throw std::runtime_error("d3d12 - invalid resource type in HLSL source");
				}

				if (resourceRegisterInsertPositions.count(setNumber) > 0 && resourceRegisterInsertPositions[setNumber].count(bindingNumber) > 0)
				{
					Log::get()->error("D3D12ShaderLoader: Found two overlapping bindings (set {}, binding {}) for resources!\n", setNumber, bindingNumber);
					throw std::runtime_error("d3d12 - overlapping shader bindings in HLSL source");
				}

				resourceRegisterInsertPositions[setNumber][bindingNumber] = std::make_tuple(pos, hlslResourceTypesMap.find(resourceTypeStr)->second, arraySize);

				pos++;
			}
		}
	}

	std::map<size_t, std::string> sourceInsertions;

	for (auto setIt = resourceRegisterInsertPositions.begin(); setIt != resourceRegisterInsertPositions.end(); setIt++)
	{
		uint32_t samplerDescCounter = 0, srvDescCounter = 0, uavDescCounter = 0, cbvDescCounter = 0;

		for (auto bindingIt = setIt->second.begin(); bindingIt != setIt->second.end(); bindingIt++)
		{
			char registerStr[128] = {0};

			switch (std::get<1>(bindingIt->second))
			{
				case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
					sprintf(registerStr, " : register(s%u, space%u)", samplerDescCounter, setIt->first);
					samplerDescCounter += std::get<2>(bindingIt->second);
					break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
					sprintf(registerStr, " : register(t%u, space%u)", srvDescCounter, setIt->first);
					srvDescCounter += std::get<2>(bindingIt->second);
					break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
					sprintf(registerStr, " : register(u%u, space%u)", uavDescCounter, setIt->first);
					uavDescCounter += std::get<2>(bindingIt->second);
					break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
					sprintf(registerStr, " : register(b%u, space%u)", cbvDescCounter, setIt->first);
					cbvDescCounter += std::get<2>(bindingIt->second);
					break;
			}

			sourceInsertions[std::get<0>(bindingIt->second)] = std::string(registerStr);
		}
	}

	size_t posOffset = 0;
	for (auto insertIt = sourceInsertions.begin(); insertIt != sourceInsertions.end(); insertIt++)
	{
		noDuplicateSpaceSource.insert(insertIt->first + posOffset, insertIt->second);
		posOffset += insertIt->second.length();
	}

	//printf("%s\n", noDuplicateSpaceSource.c_str());
	//system("pause");

	return std::vector<char>(noDuplicateSpaceSource.c_str(), noDuplicateSpaceSource.c_str() + noDuplicateSpaceSource.size());
}

std::map<std::string, D3D12_DESCRIPTOR_RANGE_TYPE> D3D12ShaderLoader_createHLSLResourceTypesMap()
{
	std::map<std::string, D3D12_DESCRIPTOR_RANGE_TYPE> map;
	map["Texture1D"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["Texture1DArray"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["Texture2D"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["Texture2DArray"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["Texture3D"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["TextureCube"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["TextureCubeArray"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["Texture2DMS"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["Texture2DMSArray"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["StructuredBuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	map["ByteAddressBuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	map["Sampler"] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	map["Sampler1D"] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	map["Sampler2D"] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	map["Sampler3D"] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	map["SamplerCube"] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	map["SamplerState"] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	map["SamplerComparisonState"] = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;

	map["RWByteAddressBuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["RWStructuredBuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["AppendStructuredBuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["ConsumeStructuredBuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["RWBuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["RWTexture1D"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["RWTexture1DArray"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["RWTexture2D"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["RWTexture2DArray"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	map["RWTexture3D"] = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;

	map["ConstantBuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	map["cbuffer"] = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;


	return map;
}

#endif