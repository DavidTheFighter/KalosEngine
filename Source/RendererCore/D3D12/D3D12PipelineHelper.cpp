#include "RendererCore/D3D12/D3D12PipelineHelper.h"

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
	pipeline->gfxPipelineInfo = pipelineInfo;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	// root sig stuff

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
	rastDesc.DepthClipEnable = !pipelineInfo.rasterizationInfo.depthClampEnable;
	rastDesc.MultisampleEnable = FALSE;
	rastDesc.AntialiasedLineEnable = FALSE;
	rastDesc.ForcedSampleCount = 0;
	rastDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = pipelineInfo.depthStencilInfo.enableDepthTest;
	depthStencilDesc.DepthWriteMask = pipelineInfo.depthStencilInfo.enableDepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	depthStencilDesc.DepthFunc = compareOpToD3D12ComparisonFunc(pipelineInfo.depthStencilInfo.depthCompareOp);
	depthStencilDesc.StencilEnable = FALSE;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.NumElements = 0;
	inputLayoutDesc.pInputElementDescs = nullptr;

	for (size_t i = 0; i < pipelineInfo.stages.size(); i++)
	{
		const PipelineShaderStage &shaderStage = pipelineInfo.stages[i];
		const D3D12ShaderModule *shaderModule = static_cast<const D3D12ShaderModule*>(shaderStage.module);

		switch (shaderStage.module->stage)
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

	printf("ph is %u\n", ResourceFormatToDXGIFormat(renderPass->attachments[renderPass->subpasses[subpass].colorAttachments[0].attachment].format));

	for (UINT i = 0; i < psoDesc.NumRenderTargets; i++)
		psoDesc.RTVFormats[i] = ResourceFormatToDXGIFormat(renderPass->attachments[renderPass->subpasses[subpass].colorAttachments[i].attachment].format);
	
	if (renderPass->subpasses[subpass].hasDepthAttachment)
		psoDesc.DSVFormat = ResourceFormatToDXGIFormat(renderPass->attachments[renderPass->subpasses[subpass].depthStencilAttachment.attachment].format);

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.NumParameters = 0;
	rootSigDesc.pParameters = nullptr;
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pStaticSamplers = nullptr;
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* signature = nullptr;
	DX_CHECK_RESULT(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));
	DX_CHECK_RESULT(renderer->device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&pipeline->rootSignature)));

	psoDesc.pRootSignature = pipeline->rootSignature;
	DX_CHECK_RESULT(renderer->device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline->pipeline)));

	signature->Release();

	return pipeline;
}