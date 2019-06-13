#include "RendererCore/D3D12/D3D12PipelineHelper.h"
#if BUILD_D3D12_BACKEND

#include <RendererCore/D3D12/D3D12Renderer.h>
#include <RendererCore/D3D12/D3D12Enums.h>
#include <RendererCore/D3D12/D3D12Objects.h>

D3D12PipelineHelper::D3D12PipelineHelper(D3D12Renderer *rendererPtr)
{
	renderer = rendererPtr;
}

D3D12PipelineHelper::~D3D12PipelineHelper()
{

}

Pipeline D3D12PipelineHelper::createGraphicsPipeline(const GraphicsPipelineInfo & pipelineInfo, RenderPass renderPass, uint32_t subpass)
{
	D3D12Pipeline *pipeline = new D3D12Pipeline();
	pipeline->pipelineBindPoint = PIPELINE_BIND_POINT_GRAPHICS;
	pipeline->gfxPipelineInfo = pipelineInfo;

	if (pipelineInfo.inputPushConstants.size > 128)
	{
		Log::get()->error("D3D12PipelineHelper: Cannot create a pipeline with more than 128 bytes of push constants");

		throw std::runtime_error("d3d12 error, inputPushConstants.size > 128");
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	D3D12_BLEND_DESC blendDesc = {};
	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = TRUE;

	for (size_t i = 0; i < pipelineInfo.colorBlendInfo.attachments.size(); i++)
	{
		const PipelineColorBlendAttachment &att = pipelineInfo.colorBlendInfo.attachments[i];
		D3D12_RENDER_TARGET_BLEND_DESC attBlendDesc = {};
		attBlendDesc.BlendEnable = BOOL(att.blendEnable);
		attBlendDesc.LogicOpEnable = BOOL(pipelineInfo.colorBlendInfo.logicOpEnable);
		attBlendDesc.SrcBlend = blendFactorToD3D12Blend(att.srcColorBlendFactor);
		attBlendDesc.DestBlend = blendFactorToD3D12Blend(att.dstColorBlendFactor);
		attBlendDesc.BlendOp = blendOpToD3D12BlendOp(att.colorBlendOp);
		attBlendDesc.SrcBlendAlpha = blendFactorToD3D12Blend(att.srcAlphaBlendFactor);
		attBlendDesc.DestBlendAlpha = blendFactorToD3D12Blend(att.dstAlphaBlendFactor);
		attBlendDesc.BlendOpAlpha = blendOpToD3D12BlendOp(att.alphaBlendOp);
		attBlendDesc.RenderTargetWriteMask = 0;

		if (att.colorWriteMask & COLOR_COMPONENT_R_BIT)
			attBlendDesc.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_RED;
		if (att.colorWriteMask & COLOR_COMPONENT_G_BIT)
			attBlendDesc.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_GREEN;
		if (att.colorWriteMask & COLOR_COMPONENT_B_BIT)
			attBlendDesc.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_BLUE;
		if (att.colorWriteMask & COLOR_COMPONENT_A_BIT)
			attBlendDesc.RenderTargetWriteMask |= D3D12_COLOR_WRITE_ENABLE_ALPHA;

		blendDesc.RenderTarget[i] = attBlendDesc;
	}

	D3D12_RASTERIZER_DESC rastDesc = {};
	rastDesc.FillMode = polygonModeToD3D12FillMode(pipelineInfo.rasterizationInfo.polygonMode);
	rastDesc.CullMode = cullModeFlagsToD3D12CullMode(pipelineInfo.rasterizationInfo.cullMode);
	rastDesc.FrontCounterClockwise = !pipelineInfo.rasterizationInfo.clockwiseFrontFace;
	rastDesc.DepthBias = 0.0f;
	rastDesc.DepthBiasClamp = 0.0f;
	rastDesc.SlopeScaledDepthBias = 0.0f;
	rastDesc.DepthClipEnable = TRUE;
	rastDesc.MultisampleEnable = pipelineInfo.rasterizationInfo.multiSampleCount > 1 ? TRUE : FALSE;
	rastDesc.AntialiasedLineEnable = FALSE;
	rastDesc.ForcedSampleCount = 0;// pipelineInfo.rasterizationInfo.multiSampleCount;
	rastDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = pipelineInfo.depthStencilInfo.enableDepthTest;
	depthStencilDesc.DepthWriteMask = pipelineInfo.depthStencilInfo.enableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = compareOpToD3D12ComparisonFunc(pipelineInfo.depthStencilInfo.depthCompareOp);
	depthStencilDesc.StencilEnable = FALSE;

	std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;

	for (size_t i = 0; i < pipelineInfo.vertexInputInfo.vertexInputAttribs.size(); i++)
	{
		const VertexInputAttribute &attrib = pipelineInfo.vertexInputInfo.vertexInputAttribs[i];

		D3D12_INPUT_ELEMENT_DESC elemDesc = {};
		elemDesc.SemanticName = "VSIN";
		elemDesc.SemanticIndex = attrib.location;
		elemDesc.Format = ResourceFormatToDXGIFormat(attrib.format);
		elemDesc.InputSlot = (UINT) attrib.location;
		elemDesc.AlignedByteOffset = attrib.offset;
		elemDesc.InputSlotClass = vertexInputRateToD3D12InputClassification(pipelineInfo.vertexInputInfo.vertexInputBindings[attrib.binding].inputRate);
		elemDesc.InstanceDataStepRate = elemDesc.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA ? 1 : 0;

		inputElements.push_back(elemDesc);
	}

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.NumElements = (UINT) inputElements.size();
	inputLayoutDesc.pInputElementDescs = inputElements.data();

	for (size_t i = 0; i < pipelineInfo.stages.size(); i++)
	{
		const PipelineShaderStage &shaderStage = pipelineInfo.stages[i];
		const D3D12ShaderModule *shaderModule = static_cast<const D3D12ShaderModule*>(shaderStage.shaderModule);

		switch (shaderStage.shaderModule->stage)
		{
			case SHADER_STAGE_VERTEX_BIT:
				psoDesc.VS = {shaderModule->shaderBytecode.get(), shaderModule->shaderBytecodeLength};
				break;
			case SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				psoDesc.HS = {shaderModule->shaderBytecode.get(), shaderModule->shaderBytecodeLength};
				break;
			case SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				psoDesc.DS = {shaderModule->shaderBytecode.get(), shaderModule->shaderBytecodeLength};
				break;
			case SHADER_STAGE_GEOMETRY_BIT:
				psoDesc.GS = {shaderModule->shaderBytecode.get(), shaderModule->shaderBytecodeLength};
				break;
			case SHADER_STAGE_FRAGMENT_BIT:
				psoDesc.PS = {shaderModule->shaderBytecode.get(), shaderModule->shaderBytecodeLength};
				break;
		}
	}

	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.BlendState = blendDesc;
	psoDesc.SampleMask = 0xFFFFFFFF;
	psoDesc.RasterizerState = rastDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.IBStripCutValue = pipelineInfo.inputAssemblyInfo.primitiveRestart ? D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.PrimitiveTopologyType = primitiveTopologyToD3D12PrimitiveTopologyType(pipelineInfo.inputAssemblyInfo.topology);
	psoDesc.NumRenderTargets = renderPass->subpasses[subpass].colorAttachments.size();
	psoDesc.NodeMask = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	switch (pipelineInfo.rasterizationInfo.multiSampleCount)
	{
		case 1:
			psoDesc.SampleDesc = {1, 0};
			break;
		case 2:
			psoDesc.SampleDesc = {2, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
			break;
		case 4:
			psoDesc.SampleDesc = {4, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
			break;
		case 8:
			psoDesc.SampleDesc = {8, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
			break;
		case 16:
			psoDesc.SampleDesc = {16, DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN};
			break;
		default:
			psoDesc.SampleDesc = {1, 0};
	}

	for (UINT i = 0; i < psoDesc.NumRenderTargets; i++)
		psoDesc.RTVFormats[i] = ResourceFormatToDXGIFormat(renderPass->attachments[renderPass->subpasses[subpass].colorAttachments[i].attachment].format);
	
	if (renderPass->subpasses[subpass].hasDepthAttachment)
		psoDesc.DSVFormat = ResourceFormatToDXGIFormat(renderPass->attachments[renderPass->subpasses[subpass].depthStencilAttachment.attachment].format);

	pipeline->rootSignature = createRootSignature(pipelineInfo.inputPushConstants, pipelineInfo.inputSetLayouts, pipelineInfo.vertexInputInfo.vertexInputAttribs.size() > 0);

	psoDesc.pRootSignature = pipeline->rootSignature;
	DX_CHECK_RESULT(renderer->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pipeline)));

	return pipeline;
}

Pipeline D3D12PipelineHelper::createComputePipeline(const ComputePipelineInfo &pipelineInfo)
{
	D3D12Pipeline *pipeline = new D3D12Pipeline();
	pipeline->pipelineBindPoint = PIPELINE_BIND_POINT_COMPUTE;
	pipeline->computePipelineInfo = pipelineInfo;

	if (pipelineInfo.inputPushConstants.size > 128)
	{
		Log::get()->error("D3D12PipelineHelper: Cannot create a pipeline with more than 128 bytes of push constants");

		throw std::runtime_error("d3d12 error, inputPushConstants.size > 128");
	}

	const D3D12ShaderModule *shaderModule = static_cast<const D3D12ShaderModule*>(pipelineInfo.shader.shaderModule);

	D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.CS = {shaderModule->shaderBytecode.get(), shaderModule->shaderBytecodeLength};
	psoDesc.NodeMask = 0;

	pipeline->rootSignature = createRootSignature(pipelineInfo.inputPushConstants, pipelineInfo.inputSetLayouts, false);

	psoDesc.pRootSignature = pipeline->rootSignature;
	DX_CHECK_RESULT(renderer->device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pipeline)));

	return pipeline;
}

ID3D12RootSignature *D3D12PipelineHelper::createRootSignature(const PushConstantRange &inputPushConstants, const std::vector<DescriptorSetLayoutDescription> &inputSetLayouts, bool allowInputAssembler)
{
	std::vector<D3D12_ROOT_PARAMETER> rootParams;
	std::vector<D3D12_STATIC_SAMPLER_DESC> staticSamplers;

	ShaderStageFlags rootSigShaderVisibility = 0;

	if (inputPushConstants.size > 0)
	{
		D3D12_ROOT_CONSTANTS rootConstants = {};
		rootConstants.ShaderRegister = 0;
		rootConstants.RegisterSpace = ROOT_CONSTANT_REGISTER_SPACE;
		rootConstants.Num32BitValues = (uint32_t)std::ceil(inputPushConstants.size / 4.0);

		D3D12_ROOT_PARAMETER rootConstantsParam = {};
		rootConstantsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootConstantsParam.Constants = rootConstants;

		switch (inputPushConstants.stageAccessFlags)
		{
			case SHADER_STAGE_VERTEX_BIT:
				rootConstantsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
				break;
			case SHADER_STAGE_TESSELLATION_CONTROL_BIT:
				rootConstantsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;
				break;
			case SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
				rootConstantsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
				break;
			case SHADER_STAGE_GEOMETRY_BIT:
				rootConstantsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
				break;
			case SHADER_STAGE_FRAGMENT_BIT:
				rootConstantsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
				break;
			case SHADER_STAGE_ALL_GRAPHICS:
			case SHADER_STAGE_ALL:
			default:
				rootConstantsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		}

		rootSigShaderVisibility |= inputPushConstants.stageAccessFlags;
		rootParams.push_back(rootConstantsParam);
	}

	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE> > samplerRanges(inputSetLayouts.size());
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE> > ranges(inputSetLayouts.size());

	for (size_t s = 0; s < inputSetLayouts.size(); s++)
	{
		const DescriptorSetLayoutDescription &setDesc = inputSetLayouts[s];

		uint32_t samplerDescCounter = 0, srvDescCounter = 0, uavDescCounter = 0, cbvDescCounter = 0;
		ShaderStageFlags samplerDescriptorStages = 0, allDescriptorStages = 0;

		std::vector<bool> samplerTypeRanges; // true if it's a static sampler, false if it's not

		for (size_t b = 0; b < setDesc.bindings.size(); b++)
		{
			const DescriptorSetBinding &binding = setDesc.bindings[b];

			switch (binding.type)
			{
				case DESCRIPTOR_TYPE_SAMPLED_TEXTURE:
				case DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
					srvDescCounter += binding.arrayCount;
					allDescriptorStages |= binding.stageAccessMask;
					break;
				case DESCRIPTOR_TYPE_STORAGE_TEXTURE:
				case DESCRIPTOR_TYPE_STORAGE_BUFFER:
					uavDescCounter += binding.arrayCount;
					allDescriptorStages |= binding.stageAccessMask;
					break;
				case DESCRIPTOR_TYPE_CONSTANT_BUFFER:
					cbvDescCounter += binding.arrayCount;
					allDescriptorStages |= binding.stageAccessMask;
					break;
				case DESCRIPTOR_TYPE_SAMPLER:
				{
					if (binding.staticSamplers.size() > 0 && binding.staticSamplers.size() != binding.arrayCount)
					{
						Log::get()->error("D3D12PipelineHelper: If a sampler binding has any number of static samplers ALL samplers in that binding must be static! (AKA staticSamplers.size() == binding.arrayCount)");
						throw std::runtime_error("d3d12 - static sampler count doesn't match sampler count in descriptor set binding");
					}

					samplerTypeRanges.insert(samplerTypeRanges.end(), binding.arrayCount, binding.staticSamplers.size() != 0);

					for (size_t sa = 0; sa < binding.staticSamplers.size(); sa++)
					{
						const DescriptorStaticSampler &setSamplerDesc = binding.staticSamplers[sa];
						D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
						samplerDesc.Filter = samplerFilterToD3D12Filter(setSamplerDesc.minFilter, setSamplerDesc.magFilter, setSamplerDesc.mipmapMode);
						samplerDesc.AddressU = samplerAddressModeToD3D12TextureAddressMode(setSamplerDesc.addressMode);
						samplerDesc.AddressV = samplerAddressModeToD3D12TextureAddressMode(setSamplerDesc.addressMode);
						samplerDesc.AddressW = samplerAddressModeToD3D12TextureAddressMode(setSamplerDesc.addressMode);
						samplerDesc.MipLODBias = setSamplerDesc.mipLodBias;
						samplerDesc.MaxAnisotropy = UINT(setSamplerDesc.maxAnisotropy);
						samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
						samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
						samplerDesc.MinLOD = setSamplerDesc.minLod;
						samplerDesc.MaxLOD = setSamplerDesc.maxLod;
						samplerDesc.ShaderRegister = samplerDescCounter + UINT(sa);
						samplerDesc.RegisterSpace = UINT(s);

						switch (binding.stageAccessMask)
						{
							case SHADER_STAGE_VERTEX_BIT:
								samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
								break;
							case SHADER_STAGE_TESSELLATION_CONTROL_BIT:
								samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;
								break;
							case SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
								samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
								break;
							case SHADER_STAGE_GEOMETRY_BIT:
								samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
								break;
							case SHADER_STAGE_FRAGMENT_BIT:
								samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
								break;
							case SHADER_STAGE_ALL_GRAPHICS:
							case SHADER_STAGE_ALL:
							default:
								samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
						}

						rootSigShaderVisibility |= binding.stageAccessMask;
						staticSamplers.push_back(samplerDesc);
					}
					
					samplerDescCounter += binding.arrayCount;
					samplerDescriptorStages |= binding.stageAccessMask;

					break;
				}
			}
		}

		bool inNonStaticSamplerRange = false;
		uint32_t nonStaticSamplerBeginRange = 0;
		for (size_t i = 0; i < samplerTypeRanges.size(); i++)
		{
			// Begin non static range
			if (!samplerTypeRanges[i] && !inNonStaticSamplerRange)
			{
				inNonStaticSamplerRange = true;
				nonStaticSamplerBeginRange = uint32_t(i);
			}

			// End non static range
			if ((samplerTypeRanges[i] && inNonStaticSamplerRange) || (!samplerTypeRanges[i] && i == samplerTypeRanges.size() - 1))
			{
				D3D12_DESCRIPTOR_RANGE samplerRange = {};
				samplerRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
				samplerRange.NumDescriptors = uint32_t(i) - nonStaticSamplerBeginRange + 1;
				samplerRange.BaseShaderRegister = nonStaticSamplerBeginRange;
				samplerRange.RegisterSpace = UINT(s);
				samplerRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				samplerRanges[s].push_back(samplerRange);
			}
		}

		rootSigShaderVisibility |= allDescriptorStages;
		rootSigShaderVisibility |= samplerDescriptorStages;

		if (samplerRanges[s].size() > 0)
		{
			D3D12_ROOT_DESCRIPTOR_TABLE descTable = {};
			descTable.NumDescriptorRanges = UINT(samplerRanges[s].size());
			descTable.pDescriptorRanges = samplerRanges[s].data();

			D3D12_ROOT_PARAMETER rootParam = {};
			rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParam.DescriptorTable = descTable;

			switch (samplerDescriptorStages)
			{
				case SHADER_STAGE_VERTEX_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
					break;
				case SHADER_STAGE_TESSELLATION_CONTROL_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;
					break;
				case SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
					break;
				case SHADER_STAGE_GEOMETRY_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
					break;
				case SHADER_STAGE_FRAGMENT_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
					break;
				case SHADER_STAGE_ALL_GRAPHICS:
				case SHADER_STAGE_ALL:
				default:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}

			rootParams.push_back(rootParam);
		}

		if (srvDescCounter > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRange.NumDescriptors = srvDescCounter;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = UINT(s);
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges[s].push_back(descRange);
		}

		if (uavDescCounter > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			descRange.NumDescriptors = uavDescCounter;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = UINT(s);
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges[s].push_back(descRange);
		}

		if (cbvDescCounter > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			descRange.NumDescriptors = cbvDescCounter;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = UINT(s);
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges[s].push_back(descRange);
		}

		if (ranges[s].size() > 0)
		{
			D3D12_ROOT_DESCRIPTOR_TABLE descTable = {};
			descTable.NumDescriptorRanges = (UINT)ranges[s].size();
			descTable.pDescriptorRanges = ranges[s].data();

			D3D12_ROOT_PARAMETER rootParam = {};
			rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParam.DescriptorTable = descTable;

			switch (allDescriptorStages)
			{
				case SHADER_STAGE_VERTEX_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
					break;
				case SHADER_STAGE_TESSELLATION_CONTROL_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_HULL;
					break;
				case SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_DOMAIN;
					break;
				case SHADER_STAGE_GEOMETRY_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_GEOMETRY;
					break;
				case SHADER_STAGE_FRAGMENT_BIT:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
					break;
				case SHADER_STAGE_ALL_GRAPHICS:
				case SHADER_STAGE_ALL:
				default:
					rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			}

			rootParams.push_back(rootParam);
		}
	}

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = (UINT)rootParams.size();
	rootSigDesc.pParameters = rootParams.data();
	rootSigDesc.NumStaticSamplers = (UINT)staticSamplers.size();
	rootSigDesc.pStaticSamplers = staticSamplers.data();
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	if (allowInputAssembler)
		rootSigDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	if (!(rootSigShaderVisibility & SHADER_STAGE_VERTEX_BIT))
		rootSigDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;

	if (!(rootSigShaderVisibility & SHADER_STAGE_TESSELLATION_CONTROL_BIT))
		rootSigDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	if (!(rootSigShaderVisibility & SHADER_STAGE_TESSELLATION_EVALUATION_BIT))
		rootSigDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;

	if (!(rootSigShaderVisibility & SHADER_STAGE_GEOMETRY_BIT))
		rootSigDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	if (!(rootSigShaderVisibility & SHADER_STAGE_FRAGMENT_BIT))
		rootSigDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC versionedRootSigDesc = {};
	versionedRootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
	versionedRootSigDesc.Desc_1_0 = rootSigDesc;

	ID3DBlob *signature = nullptr, *errorBlob = nullptr;
	ID3D12RootSignature *rootSignature = nullptr;
	HRESULT result;

	DX_CHECK_RESULT_ERR_BLOB(D3D12SerializeVersionedRootSignature(&versionedRootSigDesc, &signature, &errorBlob), errorBlob);

	if ((result = renderer->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature))) != S_OK)
	{
		_com_error err(renderer->device->GetDeviceRemovedReason());
		LPCTSTR errMsg = err.ErrorMessage();
		printf("%s\n", errMsg);

		throw std::runtime_error("FISH");
	}
	
	//signature->Release();

	return rootSignature;
}

#endif