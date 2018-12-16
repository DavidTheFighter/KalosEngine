#ifndef RENDERING_RENDERERBACKENDS_H_
#define RENDERING_RENDERERBACKENDS_H_

/*
 * All the different rendering backends that this engine has.
 */
typedef enum RendererBackend
{
	RENDERER_BACKEND_VULKAN = 0,
	RENDERER_BACKEND_D3D12 = 1,
	RENDERER_BACKEND_MAX_ENUM
} RendererBackend;

#endif /* RENDERING_RENDERERBACKENDS_H_ */
