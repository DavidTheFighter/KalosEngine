#include "RendererCore/Tests/RenderTestHandler.h"

#include <Peripherals/Window.h>

#include <RendererCore/Renderer.h>

#include <RendererCore/Tests/TriangleTest.h>
#include <RendererCore/Tests/VertexIndexBufferTest.h>
#include <RendererCore/Tests/PushConstantsTest.h>
#include <RendererCore/Tests/CubeTest.h>
#include <RendererCore/Tests/ComputeTest.h>
#include <RendererCore/Tests/MSAATest.h>

RenderTestHandler::RenderTestHandler(Renderer *rendererPtr, Window *mainWindowPtr, RendererTest testType)
{
	renderer = rendererPtr;
	currentRenderingTest = testType;
	mainWindow = mainWindowPtr;

	switch (currentRenderingTest)
	{
		case RENDERER_TEST_TRIANGLE:
			triangleTest = std::unique_ptr<TriangleTest>(new TriangleTest(renderer));

			renderer->setSwapchainTexture(mainWindow, triangleTest->gfxGraph->getRenderGraphOutputTextureView(), triangleTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
		case RENDERER_TEST_VERTEX_INDEX_BUFFER:
			vertexIndexBufferTest = std::unique_ptr<VertexIndexBufferTest>(new VertexIndexBufferTest(renderer));

			renderer->setSwapchainTexture(mainWindow, vertexIndexBufferTest->gfxGraph->getRenderGraphOutputTextureView(), vertexIndexBufferTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
		case RENDERER_TEST_PUSH_CONSTANTS:
			pushConstantsTest = std::unique_ptr<PushConstantsTest>(new PushConstantsTest(renderer));

			renderer->setSwapchainTexture(mainWindow, pushConstantsTest->gfxGraph->getRenderGraphOutputTextureView(), pushConstantsTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
		case RENDERER_TEST_CUBE:
			cubeTest = std::unique_ptr<CubeTest>(new CubeTest(renderer));

			renderer->setSwapchainTexture(mainWindow, cubeTest->gfxGraph->getRenderGraphOutputTextureView(), cubeTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
		case RENDERER_TEST_COMPUTE:
			computeTest = std::unique_ptr<ComputeTest>(new ComputeTest(renderer));

			renderer->setSwapchainTexture(mainWindow, computeTest->gfxGraph->getRenderGraphOutputTextureView(), computeTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
		case RENDERER_TEST_MSAA:
			msaaTest = std::unique_ptr<MSAATest>(new MSAATest(renderer));

			renderer->setSwapchainTexture(mainWindow, msaaTest->gfxGraph->getRenderGraphOutputTextureView(), msaaTest->renderTargetSampler, TEXTURE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			break;
	}
}

RenderTestHandler::~RenderTestHandler()
{

}

void RenderTestHandler::render()
{
	switch (currentRenderingTest)
	{
		case RENDERER_TEST_TRIANGLE:
			triangleTest->render();
			renderer->presentToSwapchain(mainWindow, {triangleTest->renderDoneSemaphore});
			break;
		case RENDERER_TEST_VERTEX_INDEX_BUFFER:
			vertexIndexBufferTest->render();
			renderer->presentToSwapchain(mainWindow, {vertexIndexBufferTest->renderDoneSemaphore});
			break;
		case RENDERER_TEST_PUSH_CONSTANTS:
			pushConstantsTest->render();
			renderer->presentToSwapchain(mainWindow, {pushConstantsTest->renderDoneSemaphore});
			break;
		case RENDERER_TEST_CUBE:
			cubeTest->render();
			renderer->presentToSwapchain(mainWindow, {cubeTest->renderDoneSemaphore});
			break;
		case RENDERER_TEST_COMPUTE:
			computeTest->render();
			renderer->presentToSwapchain(mainWindow, {computeTest->renderDoneSemaphore});
			break;
		case RENDERER_TEST_MSAA:
			msaaTest->render();
			renderer->presentToSwapchain(mainWindow, {msaaTest->renderDoneSemaphore});
			break;
	}
}