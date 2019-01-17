#ifndef RENDERING_RENDERER_RENDERERCOMMANDBUFFER_H_
#define RENDERING_RENDERER_RENDERERCOMMANDBUFFER_H_

#include <common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

struct RendererCommandPool;

class RendererCommandBuffer
{
	public:

		virtual ~RendererCommandBuffer ();

		CommandBufferLevel level;

		/*
		 * Begins recording to the command buffer. Will fail if it's already being written to. Note writing is single-threaded only.
		 */
		virtual void beginCommands (CommandBufferUsageFlags flags) = 0;

		/*
		 * Ends recording to the command buffer. Must have began to end.
		 */
		virtual void endCommands () = 0;

		//virtual void beginRenderPass (RenderPass renderPass, Framebuffer framebuffer, const Scissor &renderArea, const std::vector<ClearValue> &clearValues, SubpassContents contents) = 0;
		//virtual void endRenderPass () = 0;
		//virtual void nextSubpass (SubpassContents contents) = 0;

		virtual void bindPipeline (PipelineBindPoint point, Pipeline pipeline) = 0;

		virtual void bindIndexBuffer (Buffer buffer, size_t offset = 0, bool uses32BitIndices = false) = 0;
		virtual void bindVertexBuffers (uint32_t firstBinding, const std::vector<Buffer> &buffers, const std::vector<size_t> &offsets) = 0;

		virtual void draw (uint32_t vertexCount, uint32_t instanceCount = 1, uint32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;
		virtual void drawIndexed (uint32_t indexCount, uint32_t instanceCount = 1, uint32_t firstIndex = 0, int32_t firstVertex = 0, uint32_t firstInstance = 0) = 0;

		virtual void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) = 0;

		/*
		 * Sets the push constants to the currently bound pipeline.
		 */
		virtual void pushConstants (ShaderStageFlags stages, uint32_t offset, uint32_t size, const void *data) = 0;

		/*
		 * Binds one or more descriptor sets to the currently bound pipeline.
		 */
		virtual void bindDescriptorSets (PipelineBindPoint point, uint32_t firstSet, std::vector<DescriptorSet> sets) = 0;

		virtual void transitionTextureLayout (Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource = {0, 1, 0, 1}) = 0;
		virtual void setTextureLayout (Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource, PipelineStageFlags srcStage, PipelineStageFlags dstStage) = 0;
		virtual void stageBuffer (StagingBuffer stagingBuffer, Texture dstTexture, TextureSubresourceLayers subresource = {0, 0, 1}, sivec3 offset = {0, 0, 0}, suvec3 extent = {std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()}) = 0;
		virtual void stageBuffer (StagingBuffer stagingBuffer, Buffer dstBuffer) = 0;

		virtual void setViewports (uint32_t firstViewport, const std::vector<Viewport> &viewports) = 0;
		virtual void setScissors (uint32_t firstScissor, const std::vector<Scissor> &scissors) = 0;

		virtual void blitTexture (Texture src, TextureLayout srcLayout, Texture dst, TextureLayout dstLayout, std::vector<TextureBlitInfo> blitRegions, SamplerFilter filter = SAMPLER_FILTER_LINEAR) = 0;

#if RENDER_DEBUG_MARKERS
		virtual void beginDebugRegion (const std::string &regionName, glm::vec4 color = glm::vec4(1)) = 0;
		virtual void endDebugRegion () = 0;
		virtual void insertDebugMarker (const std::string &markerName, glm::vec4 color = glm::vec4(1)) = 0;
#else
		inline void beginDebugRegion (const std::string &regionName, glm::vec4 color = glm::vec4(1))
		{};
		inline void endDebugRegion ()
		{};
		inline void insertDebugMarker (const std::string &markerName, glm::vec4 color = glm::vec4(1))
		{};
#endif
};

typedef RendererCommandBuffer *CommandBuffer;

#endif /* RENDERING_RENDERER_RENDERERCOMMANDBUFFER_H_ */
