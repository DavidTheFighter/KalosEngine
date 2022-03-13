#include "Renderer/NuklearGUIRenderer.h"

#include <RendererCore/Renderer.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#include <nuklear.h>

typedef struct
{
	glm::mat4 projMat;
	float guiElementDepth;
} GUIRendererPushConstants;

NuklearGUIRenderer::NuklearGUIRenderer(Renderer* rendererPtr, RenderPass renderPass, DescriptorSet whiteTexturePtr, uint64_t streamingBufferSizeKb)
{
	renderer = rendererPtr;
	guiGfxPipeline = nullptr;
	whiteTextureDescriptorSet = whiteTexturePtr;
	guiStreamBufferIndex = 0;
	guiStreamBufferSize = streamingBufferSizeKb * 1024;

	createGraphicsPipeline(renderPass);

	guiVertexIndexStreamBuffer = renderer->createBuffer(guiStreamBufferSize, BUFFER_USAGE_VERTEX_BUFFER_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT, BUFFER_LAYOUT_VERTEX_INDEX_BUFFER, MEMORY_USAGE_CPU_TO_GPU, false);
	guiStreamBufferData = reinterpret_cast<uint8_t*>(renderer->mapBuffer(guiVertexIndexStreamBuffer));

	nkCmdsBuffer = std::unique_ptr<struct nk_buffer> (new nk_buffer());
	nkVerticesBuffer = std::unique_ptr<struct nk_buffer>(new nk_buffer());
	nkIndicesBuffer = std::unique_ptr<struct nk_buffer>(new nk_buffer());

	nk_buffer_init_default(nkCmdsBuffer.get());
	nk_buffer_init_default(nkVerticesBuffer.get());
	nk_buffer_init_default(nkIndicesBuffer.get());
}

NuklearGUIRenderer::~NuklearGUIRenderer()
{
	renderer->unmapBuffer(guiVertexIndexStreamBuffer);
	renderer->destroyBuffer(guiVertexIndexStreamBuffer);
	renderer->destroyPipeline(guiGfxPipeline);

	nk_buffer_free(nkCmdsBuffer.get());
	nk_buffer_free(nkVerticesBuffer.get());
	nk_buffer_free(nkIndicesBuffer.get());
}

/*
 * A draw command plus some extra data for this engine to batch/order the
 * draw commands in an efficient manner.
 */
struct nk_draw_command_extended
{
	struct nk_draw_command nkCmd; // The original nk_draw_command
	uint32_t indexOffset;  // The offset to the index buffer given
	uint32_t cmdIndex;     // The index of when the command was given
};

const struct nk_draw_vertex_layout_element vertex_layout[] = {
		{NK_VERTEX_POSITION, NK_FORMAT_FLOAT, offsetof(nk_vertex, vertex)}, 
		{NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, offsetof(nk_vertex, uv)},
		{NK_VERTEX_COLOR, NK_FORMAT_R32G32B32A32_FLOAT, offsetof(nk_vertex, color)}, 
		{NK_VERTEX_LAYOUT_END}
	};

void NuklearGUIRenderer::renderNuklearGUI(CommandBuffer cmdBuffer, struct nk_context& ctx, uint32_t renderWidth, uint32_t renderHeight)
{
	nk_convert_config cfg = {};
	cfg.shape_AA = NK_ANTI_ALIASING_ON;
	cfg.line_AA = NK_ANTI_ALIASING_ON;
	cfg.vertex_layout = vertex_layout;
	cfg.vertex_size = sizeof(nk_vertex);
	cfg.vertex_alignment = NK_ALIGNOF(nk_vertex);
	cfg.circle_segment_count = 22;
	cfg.curve_segment_count = 22;
	cfg.arc_segment_count = 22;
	cfg.global_alpha = 1.0f;
	cfg.null = {nk_handle_ptr(whiteTextureDescriptorSet), {0, 0}};
	
	nk_convert(&ctx, nkCmdsBuffer.get(), nkVerticesBuffer.get(), nkIndicesBuffer.get(), &cfg);

	// Stream vertices
	if (guiStreamBufferIndex + nkVerticesBuffer->memory.size >= guiStreamBufferSize)
		guiStreamBufferIndex = 0;

	uint64_t streamBufferVertexOffset = guiStreamBufferIndex;
	guiStreamBufferIndex += nkVerticesBuffer->memory.size;

	memcpy(guiStreamBufferData + streamBufferVertexOffset, nkVerticesBuffer->memory.ptr, nkVerticesBuffer->memory.size);

	// Stream indices
	if (guiStreamBufferIndex + nkIndicesBuffer->memory.size >= guiStreamBufferSize)
		guiStreamBufferIndex = 0;

	uint64_t streamBufferIndexOffset = guiStreamBufferIndex;
	guiStreamBufferIndex += nkIndicesBuffer->memory.size;

	memcpy(guiStreamBufferData + streamBufferIndexOffset, nkIndicesBuffer->memory.ptr, nkIndicesBuffer->memory.size);

	cmdBuffer->bindPipeline(PIPELINE_BIND_POINT_GRAPHICS, guiGfxPipeline);

	GUIRendererPushConstants pushConsts = {};
	pushConsts.projMat = glm::ortho<float>(0, renderWidth, 0, renderHeight);
	pushConsts.guiElementDepth = 0;

	cmdBuffer->pushConstants(0, sizeof(pushConsts), &pushConsts);

	cmdBuffer->bindIndexBuffer(guiVertexIndexStreamBuffer, streamBufferIndexOffset, false);
	cmdBuffer->bindVertexBuffers(0, {guiVertexIndexStreamBuffer}, {streamBufferVertexOffset});

	// The list of draw commands, sorted by texture
	std::map<uint64_t, std::vector<nk_draw_command_extended> > drawCommands;

	uint32_t offset = 0;
	uint32_t cmdIndex = 0;

	const nk_draw_command* cmd;
	nk_draw_foreach(cmd, &ctx, nkCmdsBuffer.get())
	{
		if (!cmd->elem_count)
			continue;

		nk_draw_command_extended cmd_extended;
		cmd_extended.nkCmd = *cmd;
		cmd_extended.indexOffset = offset;
		cmd_extended.cmdIndex = cmdIndex;

		drawCommands[cmdIndex].push_back(cmd_extended);

		offset += cmd->elem_count;
		cmdIndex++;
	}

	uint32_t drawCommandCount = cmdIndex;

	DescriptorSet currentDrawCommandTexture = nullptr;

	for (auto drawCommandIt = drawCommands.begin(); drawCommandIt != drawCommands.end(); drawCommandIt++)
	{
		for (size_t i = 0; i < drawCommandIt->second.size(); i++)
		{
			cmd = &drawCommandIt->second[i].nkCmd;
			offset = drawCommandIt->second[i].indexOffset;
			cmdIndex = drawCommandIt->second[i].cmdIndex;

			Scissor cmdScissor;
			cmdScissor.x = (int32_t)glm::clamp(cmd->clip_rect.x, 0.0f, (float)renderWidth);
			cmdScissor.y = (int32_t)glm::clamp(cmd->clip_rect.y, 0.0f, (float)renderHeight);
			cmdScissor.width = (uint32_t)glm::clamp(cmd->clip_rect.w, 0.0f, (float)renderWidth);
			cmdScissor.height = (uint32_t)glm::clamp(cmd->clip_rect.h, 0.0f, (float)renderHeight);

			if (cmd->texture.ptr == nullptr)
			{
				Log::get()->error("NuklearGUIRenderer: Tried rendering a GUI element with a texture == nullptr!");
				throw std::runtime_error("NuklearGUIRenderer: Tried rendering a GUI element with a texture == nullptr!");
			}

			if (currentDrawCommandTexture != static_cast<DescriptorSet>(cmd->texture.ptr))
			{
				cmdBuffer->bindDescriptorSets(PIPELINE_BIND_POINT_GRAPHICS, 0, {static_cast<DescriptorSet>(cmd->texture.ptr)});
				currentDrawCommandTexture = static_cast<DescriptorSet>(cmd->texture.ptr);
			}

			pushConsts.guiElementDepth = (float(cmdIndex) / float(drawCommandCount)) - 1.0f;

			cmdBuffer->setScissors(0, {cmdScissor});
			cmdBuffer->pushConstants(offsetof(GUIRendererPushConstants, guiElementDepth), sizeof(pushConsts.guiElementDepth), &pushConsts.guiElementDepth);
			
			cmdBuffer->drawIndexed(cmd->elem_count, 1, offset, 0, 0);
		}
	}

	nk_buffer_clear(nkCmdsBuffer.get());
	nk_buffer_clear(nkVerticesBuffer.get());
	nk_buffer_clear(nkIndicesBuffer.get());
}

void NuklearGUIRenderer::createGraphicsPipeline(RenderPass renderPass)
{
	ShaderModule vertShader = renderer->createShaderModule("GameData/shaders/nuklear-gui.hlsl", SHADER_STAGE_VERTEX_BIT, SHADER_LANGUAGE_HLSL, "NuklearGUIVS");
	ShaderModule fragShader = renderer->createShaderModule("GameData/shaders/nuklear-gui.hlsl", SHADER_STAGE_FRAGMENT_BIT, SHADER_LANGUAGE_HLSL, "NuklearGUIFS");

	PipelineShaderStage vertShaderStage = {};
	vertShaderStage.shaderModule = vertShader;

	PipelineShaderStage fragShaderStage = {};
	fragShaderStage.shaderModule = fragShader;

	std::vector<VertexInputBinding> bindingDesc = std::vector<VertexInputBinding>(1);
	bindingDesc[0].binding = 0;
	bindingDesc[0].stride = static_cast<uint32_t>(sizeof(nk_vertex));
	bindingDesc[0].inputRate = VERTEX_INPUT_RATE_VERTEX;

	std::vector<VertexInputAttribute> attribDesc = std::vector<VertexInputAttribute>(3);
	attribDesc[0].binding = 0;
	attribDesc[0].location = 0;
	attribDesc[0].format = RESOURCE_FORMAT_R32G32_SFLOAT;
	attribDesc[0].offset = static_cast<uint32_t>(offsetof(nk_vertex, vertex));

	attribDesc[1].binding = 0;
	attribDesc[1].location = 1;
	attribDesc[1].format = RESOURCE_FORMAT_R32G32_SFLOAT;
	attribDesc[1].offset = static_cast<uint32_t>(offsetof(nk_vertex, uv));

	attribDesc[2].binding = 0;
	attribDesc[2].location = 2;
	attribDesc[2].format = RESOURCE_FORMAT_R32G32B32A32_SFLOAT;
	attribDesc[2].offset = static_cast<uint32_t>(offsetof(nk_vertex, color));

	PipelineVertexInputInfo vertexInput = {};
	vertexInput.vertexInputBindings = bindingDesc;
	vertexInput.vertexInputAttribs = attribDesc;

	PipelineInputAssemblyInfo inputAssembly = {};
	inputAssembly.primitiveRestart = false;
	inputAssembly.topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	PipelineRasterizationInfo rastInfo = {};
	rastInfo.clockwiseFrontFace = false;
	rastInfo.cullMode = POLYGON_CULL_MODE_NONE;
	rastInfo.polygonMode = POLYGON_MODE_FILL;
	rastInfo.enableOutOfOrderRasterization = false;

	PipelineDepthStencilInfo depthInfo = {};
	depthInfo.enableDepthTest = true;
	depthInfo.enableDepthWrite = true;
	depthInfo.depthCompareOp = COMPARE_OP_LESS_OR_EQUAL;

	PipelineColorBlendAttachment colorBlendAttachment = {};
	colorBlendAttachment.blendEnable = true;
	colorBlendAttachment.colorWriteMask = COLOR_COMPONENT_R_BIT | COLOR_COMPONENT_G_BIT | COLOR_COMPONENT_B_BIT | COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.colorBlendOp = BLEND_OP_ADD;
	colorBlendAttachment.alphaBlendOp = BLEND_OP_ADD;
	colorBlendAttachment.srcAlphaBlendFactor = BLEND_FACTOR_ONE;
	colorBlendAttachment.dstAlphaBlendFactor = BLEND_FACTOR_ZERO;
	colorBlendAttachment.srcColorBlendFactor = BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachment.dstColorBlendFactor = BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	PipelineColorBlendInfo colorBlend = {};
	colorBlend.attachments = {colorBlendAttachment};
	colorBlend.logicOpEnable = false;

	DescriptorStaticSampler staticSampler = {};
	staticSampler.magFilter = SAMPLER_FILTER_LINEAR;
	staticSampler.maxAnisotropy = 1.0f;

	DescriptorSetBinding guiSamplerBinding = {};
	guiSamplerBinding.binding = 0;
	guiSamplerBinding.arrayCount = 1;
	guiSamplerBinding.stageAccessMask = SHADER_STAGE_FRAGMENT_BIT;
	guiSamplerBinding.type = DESCRIPTOR_TYPE_SAMPLER;
	guiSamplerBinding.staticSamplers = {staticSampler};

	DescriptorSetBinding guiTextureBinding = {};
	guiTextureBinding.binding = 1;
	guiTextureBinding.arrayCount = 1;
	guiTextureBinding.stageAccessMask = SHADER_STAGE_FRAGMENT_BIT;
	guiTextureBinding.type = DESCRIPTOR_TYPE_SAMPLED_TEXTURE;

	DescriptorSetLayoutDescription set0 = {};
	set0.bindings = {guiSamplerBinding, guiTextureBinding};

	GraphicsPipelineInfo info = {};
	info.stages = {vertShaderStage, fragShaderStage};
	info.vertexInputInfo = vertexInput;
	info.inputAssemblyInfo = inputAssembly;
	info.rasterizationInfo = rastInfo;
	info.depthStencilInfo = depthInfo;
	info.colorBlendInfo = colorBlend;

	info.inputPushConstants = {sizeof(GUIRendererPushConstants), SHADER_STAGE_VERTEX_BIT | SHADER_STAGE_FRAGMENT_BIT};
	info.inputSetLayouts = {set0};

	guiGfxPipeline = renderer->createGraphicsPipeline(info, renderPass, 0);

	renderer->destroyShaderModule(vertShaderStage.shaderModule);
	renderer->destroyShaderModule(fragShaderStage.shaderModule);
}
