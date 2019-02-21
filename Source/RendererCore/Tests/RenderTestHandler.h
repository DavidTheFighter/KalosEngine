#ifndef RENDERERCORE_TESTS_RENDERTESTHANDLER_H_
#define RENDERERCORE_TESTS_RENDERTESTHANDLER_H_

#include <RendererCore/renderer_common.h>

#include <RendererCore/RendererEnums.h>
#include <RendererCore/RendererObjects.h>

typedef enum
{
	RENDERER_TEST_TRIANGLE, // Tests the bare basics, as little as possible to get something rendered to the screen
	RENDERER_TEST_VERTEX_INDEX_BUFFER, // Tests vertex and index buffers by drawing two triangles
	RENDERER_TEST_PUSH_CONSTANTS, // Tests push constants by making a 3D rotating triangle
	RENDERER_TEST_CUBE, // Combines vertex/index buffer, push constants, depth buffer + more to make a rotating cube like the vkcube.exe demo
	RENDERER_TEST_COMPUTE, // Tests compute shaders, pipelines, resource binding for compute
	RENDERER_TEST_MSAA
} RendererTest;

class Renderer;
class Window;

class TriangleTest;
class VertexIndexBufferTest;
class PushConstantsTest;
class CubeTest;
class ComputeTest;
class MSAATest;

class RenderTestHandler
{
	public:

	RenderTestHandler(Renderer *rendererPtr, Window *mainWindowPtr, RendererTest testType);
	virtual ~RenderTestHandler();

	void render();

	private:

	Renderer *renderer;
	Window *mainWindow;

	RendererTest currentRenderingTest;

	std::unique_ptr<TriangleTest> triangleTest;
	std::unique_ptr<VertexIndexBufferTest> vertexIndexBufferTest;
	std::unique_ptr<PushConstantsTest> pushConstantsTest;
	std::unique_ptr<CubeTest> cubeTest;
	std::unique_ptr<ComputeTest> computeTest;
	std::unique_ptr<MSAATest> msaaTest;

};

#endif /* RENDERERCORE_TESTS_RENDERTESTHANDLER_H_ */