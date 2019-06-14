#ifndef RENDERER_GUIRENDERER_H_
#define RENDERER_GUIRENDERER_H_

#include <common.h>
#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

class Renderer;

typedef struct
{
	svec2 vertex;
	svec2 uv;
	svec4 color;
} nk_vertex;

typedef struct
{
	Texture fontTexture;
	TextureView fontTextureView;
	DescriptorSet fontTextureDescriptorSet;

	struct nk_font *nkFont;

} NkFontData;

class NuklearGUIRenderer
{
public:
	NuklearGUIRenderer(Renderer *rendererPtr, RenderPass renderPass, DescriptorSet whiteTexturePtr, uint64_t streamingBufferSizeKb = 512);
	virtual ~NuklearGUIRenderer();

	void renderNuklearGUI(CommandBuffer cmdBuffer, struct nk_context &ctx, uint32_t renderWidth, uint32_t renderHeight);

private:
	Renderer *renderer;

	Buffer guiVertexIndexStreamBuffer;
	uint8_t *guiStreamBufferData;
	uint64_t guiStreamBufferIndex;
	uint64_t guiStreamBufferSize;

	Pipeline guiGfxPipeline;

	DescriptorSet whiteTextureDescriptorSet;

	std::unique_ptr<struct nk_buffer> nkCmdsBuffer;
	std::unique_ptr<struct nk_buffer> nkVerticesBuffer;
	std::unique_ptr<struct nk_buffer> nkIndicesBuffer;

	void createGraphicsPipeline(RenderPass renderPass);
};

#endif /* RENDERER_GUIRENDERER_H_ */