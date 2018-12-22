#ifndef RENDERING_D3D12_D3D12SWAPCHAIN_H_
#define RENDERING_D3D12_D3D12SWAPCHAIN_H_

#include <RendererCore/D3D12/d3d12_common.h>

#include <Peripherals/Window.h>

class D3D12Renderer;

typedef struct
{
	Window *parentWindow;

	IDXGISwapChain4 *dxgiSwapchain4;
	ID3D12DescriptorHeap *descHeap;

	std::vector<ID3D12Resource*> backBuffers;
	ID3D12CommandAllocator *cmdAllocator;
	std::vector<ID3D12GraphicsCommandList*> cmdLists;

	ID3D12Fence *swapchainFence;
	UINT64 swapchainFenceValue;
	std::vector<UINT64> bufferFenceValues;

	uint32_t currentBackBufferIndex;

	HANDLE fenceEvent;

} D3D12SwapchainData;

class D3D12SwapchainHandler
{
	public:

	D3D12SwapchainHandler(D3D12Renderer *rendererPtr);
	virtual ~D3D12SwapchainHandler();

	void initSwapchain(Window *wnd);
	void presentToSwapchain(Window *wnd);

	void createSwapchain(Window *wnd);
	void destroySwapchain(Window *wnd);
	void recreateSwapchain(Window *wnd);

	private:

	D3D12Renderer *renderer;

	std::map<Window*, D3D12SwapchainData*> swapchains;

	bool supportsTearing;

	IDXGIFactory4 *dxgiFactory4;
	UINT rtvDescriptorSize;

	ID3D12PipelineState *swapchainPSO;
	ID3D12RootSignature *swapchainRootSig;
	ID3D12Resource *swapchainVertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW swapchainVertexBufferView;

	ID3D12DescriptorHeap *swapchainDescHeap;
	ID3D12Resource *swapchainCBufferHeap;
	ID3D12Resource *swapchainTextureHeap;

	ID3D12CommandAllocator *swapchainUtilCommandAlloc;
	ID3D12Fence *swapchainUtilFence;
	HANDLE swapchainUtilFenceEvent;
	uint64_t swapchainUtilFenceValue;

	void prerecordSwapchainCommandList(D3D12SwapchainData *swapchain, ID3D12GraphicsCommandList *cmdList, uint32_t bufferIndex);

	void createRootSignature();
	void createPSO();
	void createVertexBuffer();
	void createTexture();
	void createConstantBuffer();
};

#endif /* RENDERING_D3D12_D3D12SWAPCHAIN_H_ */