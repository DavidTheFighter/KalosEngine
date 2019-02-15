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
	blendDesc.IndependentBlendEnable = FALSE;

	for (size_t i = 0; i < pipelineInfo.colorBlendInfo.attachments.size(); i++)
	{
		const PipelineColorBlendAttachment &att = pipelineInfo.colorBlendInfo.attachments[i];
		D3D12_RENDER_TARGET_BLEND_DESC attBlendDesc = {};
		attBlendDesc.BlendEnable = BOOL(att.blendEnable);
		attBlendDesc.LogicOpEnable = BOOL(pipelineInfo.colorBlendInfo.logicOpEnable);
		attBlendDesc.SrcBlend = blendFactorToD3D12Blend(att.srcColorBlendFactor);
		attBlendDesc.DestBlend = blendFactorToD3D12Blend(att.dstColorBlendFactor);
		attBlendDesc.BlendOp = blendOpToD3D12BlendOp(att.colorBlendOp);
		attBlendDesc.SrcBlendAlpha = blendFactorToD3D12Blend(att.srcColorBlendFactor);
		attBlendDesc.DestBlendAlpha = blendFactorToD3D12Blend(att.srcAlphaBlendFactor);
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
	rastDesc.MultisampleEnable = FALSE;
	rastDesc.AntialiasedLineEnable = FALSE;
	rastDesc.ForcedSampleCount = 0;
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
	psoDesc.SampleMask = 0xFFFFFF; // TODO Figure out what this actually does xD
	psoDesc.RasterizerState = rastDesc;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.IBStripCutValue = pipelineInfo.inputAssemblyInfo.primitiveRestart ? D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_0xFFFF : D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	psoDesc.PrimitiveTopologyType = primitiveTopologyToD3D12PrimitiveTopologyType(pipelineInfo.inputAssemblyInfo.topology);
	psoDesc.NumRenderTargets = renderPass->subpasses[subpass].colorAttachments.size();
	psoDesc.SampleDesc = {1, 0};
	psoDesc.NodeMask = 0;
	psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	for (UINT i = 0; i < psoDesc.NumRenderTargets; i++)
		psoDesc.RTVFormats[i] = ResourceFormatToDXGIFormat(renderPass->attachments[renderPass->subpasses[subpass].colorAttachments[i].attachment].format);
	
	if (renderPass->subpasses[subpass].hasDepthAttachment)
		psoDesc.DSVFormat = ResourceFormatToDXGIFormat(renderPass->attachments[renderPass->subpasses[subpass].depthStencilAttachment.attachment].format);

	std::vector<D3D12_ROOT_PARAMETER> rootParams;

	if (pipelineInfo.inputPushConstants.size > 0)
	{
		D3D12_ROOT_CONSTANTS rootConstants = {};
		rootConstants.ShaderRegister = 0;
		rootConstants.RegisterSpace = ROOT_CONSTANT_REGISTER_SPACE;
		rootConstants.Num32BitValues = (uint32_t) std::ceil(pipelineInfo.inputPushConstants.size / 4.0);

		D3D12_ROOT_PARAMETER rootConstantsParam = {};
		rootConstantsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootConstantsParam.Constants = rootConstants;
		
		switch (pipelineInfo.inputPushConstants.stageAccessFlags)
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

		rootParams.push_back(rootConstantsParam);
	}

	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> allSamplerRanges(pipelineInfo.inputSetLayouts.size());
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> allRanges(pipelineInfo.inputSetLayouts.size());

	for (size_t s = 0; s < pipelineInfo.inputSetLayouts.size(); s++)
	{
		const DescriptorSetLayoutDescription &setDesc = pipelineInfo.inputSetLayouts[s];
		
		std::vector<D3D12_DESCRIPTOR_RANGE> &samplerRanges = allSamplerRanges[s];
		ShaderStageFlags samplerDescriptorStages = 0;

		std::vector<D3D12_DESCRIPTOR_RANGE> &ranges = allRanges[s];
		ShaderStageFlags allDescriptorStages = 0;

		if (setDesc.samplerDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			descRange.NumDescriptors = setDesc.samplerDescriptorCount;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			samplerRanges.push_back(descRange);

			for (size_t d = 0; d < setDesc.samplerDescriptorCount; d++)
				samplerDescriptorStages |= setDesc.samplerBindingsShaderStageAccess[d];
		}

		if (setDesc.constantBufferDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			descRange.NumDescriptors = setDesc.constantBufferDescriptorCount;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.push_back(descRange);

			for (size_t d = 0; d < setDesc.constantBufferDescriptorCount; d++)
				allDescriptorStages |= setDesc.constantBufferBindingsShaderStageAccess[d];
		}

		if (setDesc.inputAttachmentDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRange.NumDescriptors = setDesc.inputAttachmentDescriptorCount;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.push_back(descRange);

			for (size_t d = 0; d < setDesc.inputAttachmentDescriptorCount; d++)
				allDescriptorStages |= setDesc.inputAttachmentBindingsShaderStageAccess[d];
		}

		if (setDesc.textureDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRange.NumDescriptors = setDesc.textureDescriptorCount;
			descRange.BaseShaderRegister = 10;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.push_back(descRange);

			for (size_t d = 0; d < setDesc.textureDescriptorCount; d++)
				allDescriptorStages |= setDesc.textureBindingsShaderStageAccess[d];
		}

		if (ranges.size() > 0)
		{
			D3D12_ROOT_DESCRIPTOR_TABLE descTable = {};
			descTable.NumDescriptorRanges = (UINT) ranges.size();
			descTable.pDescriptorRanges = ranges.data();

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

		if (samplerRanges.size() > 0)
		{
			D3D12_ROOT_DESCRIPTOR_TABLE descTable = {};
			descTable.NumDescriptorRanges = (UINT) samplerRanges.size();
			descTable.pDescriptorRanges = samplerRanges.data();

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
	}

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = (UINT) rootParams.size();
	rootSigDesc.pParameters = rootParams.data();
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	if (pipelineInfo.vertexInputInfo.vertexInputAttribs.size() > 0)
		rootSigDesc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signature = nullptr;
	DX_CHECK_RESULT(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));
	DX_CHECK_RESULT(renderer->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pipeline->rootSignature)));

	psoDesc.pRootSignature = pipeline->rootSignature;
	DX_CHECK_RESULT(renderer->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pipeline)));

	signature->Release();

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

	std::vector<D3D12_ROOT_PARAMETER> rootParams;

	if (pipelineInfo.inputPushConstants.size > 0)
	{
		D3D12_ROOT_CONSTANTS rootConstants = {};
		rootConstants.ShaderRegister = 0;
		rootConstants.RegisterSpace = ROOT_CONSTANT_REGISTER_SPACE;
		rootConstants.Num32BitValues = (uint32_t) std::ceil(pipelineInfo.inputPushConstants.size / 4.0);

		D3D12_ROOT_PARAMETER rootConstantsParam = {};
		rootConstantsParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		rootConstantsParam.Constants = rootConstants;
		rootConstantsParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

#if D3D12_DEBUG_COMPATIBILITY_CHECKS
		if (pipelineInfo.inputPushConstants.stageAccessFlags != SHADER_STAGE_COMPUTE_BIT && pipelineInfo.inputPushConstants.stageAccessFlags != SHADER_STAGE_ALL)
			Log::get()->error("D3D12PipelineHelper: Push constant visibility can only be SHADER_STAGE_COMPUTE_BIT or SHADER_STAGE_ALL while used in a compute pipeline");
#endif

		rootParams.push_back(rootConstantsParam);
	}

	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> allSamplerRanges(pipelineInfo.inputSetLayouts.size());
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> allRanges(pipelineInfo.inputSetLayouts.size());

	for (size_t s = 0; s < pipelineInfo.inputSetLayouts.size(); s++)
	{
		const DescriptorSetLayoutDescription &setDesc = pipelineInfo.inputSetLayouts[s];

		std::vector<D3D12_DESCRIPTOR_RANGE> &samplerRanges = allSamplerRanges[s];
		ShaderStageFlags samplerDescriptorStages = 0;

		std::vector<D3D12_DESCRIPTOR_RANGE> &ranges = allRanges[s];
		ShaderStageFlags allDescriptorStages = 0;

		if (setDesc.samplerDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
			descRange.NumDescriptors = setDesc.samplerDescriptorCount;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			samplerRanges.push_back(descRange);

			for (size_t d = 0; d < setDesc.samplerDescriptorCount; d++)
				samplerDescriptorStages |= setDesc.samplerBindingsShaderStageAccess[d];
		}

		if (setDesc.constantBufferDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
			descRange.NumDescriptors = setDesc.constantBufferDescriptorCount;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.push_back(descRange);

			for (size_t d = 0; d < setDesc.constantBufferDescriptorCount; d++)
				allDescriptorStages |= setDesc.constantBufferBindingsShaderStageAccess[d];
		}

		if (setDesc.inputAttachmentDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRange.NumDescriptors = setDesc.inputAttachmentDescriptorCount;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.push_back(descRange);

			for (size_t d = 0; d < setDesc.inputAttachmentDescriptorCount; d++)
				allDescriptorStages |= setDesc.inputAttachmentBindingsShaderStageAccess[d];
		}

		if (setDesc.storageTextureDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
			descRange.NumDescriptors = setDesc.storageTextureDescriptorCount;
			descRange.BaseShaderRegister = 0;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.push_back(descRange);

			for (size_t d = 0; d < setDesc.storageTextureDescriptorCount; d++)
				allDescriptorStages |= setDesc.storageTextureBindingsShaderStageAccess[d];
		}

		if (setDesc.textureDescriptorCount > 0)
		{
			D3D12_DESCRIPTOR_RANGE descRange = {};
			descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			descRange.NumDescriptors = setDesc.textureDescriptorCount;
			descRange.BaseShaderRegister = 10;
			descRange.RegisterSpace = (UINT) s;
			descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

			ranges.push_back(descRange);

			for (size_t d = 0; d < setDesc.textureDescriptorCount; d++)
				allDescriptorStages |= setDesc.textureBindingsShaderStageAccess[d];
		}

		if (ranges.size() > 0)
		{
			D3D12_ROOT_DESCRIPTOR_TABLE descTable = {};
			descTable.NumDescriptorRanges = (UINT) ranges.size();
			descTable.pDescriptorRanges = ranges.data();

			D3D12_ROOT_PARAMETER rootParam = {};
			rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParam.DescriptorTable = descTable;
			rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

#if D3D12_DEBUG_COMPATIBILITY_CHECKS
			if (allDescriptorStages != SHADER_STAGE_COMPUTE_BIT && pipelineInfo.inputPushConstants.stageAccessFlags != SHADER_STAGE_ALL)
				Log::get()->error("D3D12PipelineHelper: Descriptor visibility can only be SHADER_STAGE_COMPUTE_BIT or SHADER_STAGE_ALL while used in a compute pipeline");
#endif

			rootParams.push_back(rootParam);
		}

		if (samplerRanges.size() > 0)
		{
			D3D12_ROOT_DESCRIPTOR_TABLE descTable = {};
			descTable.NumDescriptorRanges = (UINT) samplerRanges.size();
			descTable.pDescriptorRanges = samplerRanges.data();

			D3D12_ROOT_PARAMETER rootParam = {};
			rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			rootParam.DescriptorTable = descTable;
			rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

#if D3D12_DEBUG_COMPATIBILITY_CHECKS
			if (samplerDescriptorStages != SHADER_STAGE_COMPUTE_BIT && pipelineInfo.inputPushConstants.stageAccessFlags != SHADER_STAGE_ALL)
				Log::get()->error("D3D12PipelineHelper: Sampler visibility can only be SHADER_STAGE_COMPUTE_BIT or SHADER_STAGE_ALL while used in a compute pipeline");
#endif

			rootParams.push_back(rootParam);
		}
	}

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = (UINT) rootParams.size();
	rootSigDesc.pParameters = rootParams.data();
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* signature = nullptr;
	DX_CHECK_RESULT(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));
	DX_CHECK_RESULT(renderer->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pipeline->rootSignature)));

	psoDesc.pRootSignature = pipeline->rootSignature;
	DX_CHECK_RESULT(renderer->device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pipeline)));

	signature->Release();

	return pipeline;
}

#endif