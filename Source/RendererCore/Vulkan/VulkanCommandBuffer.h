
#ifndef RENDERING_VULKAN_VULKANCOMMANDBUFFER_H_
#define RENDERING_VULKAN_VULKANCOMMANDBUFFER_H_

#include <RendererCore/Vulkan/vulkan_common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class VulkanCommandBuffer : public RendererCommandBuffer
{
	public:

		VkCommandBuffer bufferHandle;

		VulkanCommandBuffer ();
		virtual ~VulkanCommandBuffer ();

		void beginCommands (CommandBufferUsageFlags flags);
		void endCommands ();

		void bindPipeline (PipelineBindPoint point, Pipeline pipeline);

		void bindIndexBuffer (Buffer buffer, size_t offset, bool uses32BitIndices);
		void bindVertexBuffers (uint32_t firstBinding, const std::vector<Buffer> &buffers, const std::vector<size_t> &offsets);

		void draw (uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance);
		void drawIndexed (uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t firstVertex, uint32_t firstInstance);
		void dispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);

		void pushConstants (uint32_t offset, uint32_t size, const void *data);
		void bindDescriptorSets (PipelineBindPoint point, uint32_t firstSet, const std::vector<DescriptorSet> &sets);

		void transitionTextureLayout (Texture texture, TextureLayout oldLayout, TextureLayout newLayout, TextureSubresourceRange subresource);

		void stageBuffer (StagingBuffer stagingBuffer, Texture dstTexture, TextureSubresourceLayers subresource, sivec3 offset, suvec3 extent);
		void stageBuffer (StagingBuffer stagingBuffer, Buffer dstBuffer);
		void stageTextureSubresources(StagingTexture stagingTexture, Texture dstTexture, TextureSubresourceRange subresources);

		void setViewports (uint32_t firstViewport, const std::vector<Viewport> &viewports);
		void setScissors (uint32_t firstScissor, const std::vector<Scissor> &scissors);

		void blitTexture (Texture src, TextureLayout srcLayout, Texture dst, TextureLayout dstLayout, std::vector<TextureBlitInfo> blitRegions, SamplerFilter filter);

		void beginDebugRegion (const std::string &regionName, glm::vec4 color);
		void endDebugRegion ();
		void insertDebugMarker (const std::string &markerName, glm::vec4 color);

		void vulkan_beginRenderPass(VkRenderPass renderPass, VkFramebuffer framebuffer, const VkRect2D &renderArea, const std::vector<VkClearValue> &clearValues, VkSubpassContents contents);
		void vulkan_endRenderPass();
		void vulkan_nextSubpass(VkSubpassContents contents);
		void vulkan_resetCommands();

	private:

		Pipeline context_currentBoundPipeline;

		bool startedRecording;

		friend class VulkanRenderer;
};

#endif /* RENDERING_VULKAN_VULKANCOMMANDBUFFER_H_ */
