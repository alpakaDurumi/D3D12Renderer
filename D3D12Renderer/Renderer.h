#pragma once

#include <string>
#include <Windows.h>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6
#include <vector>

using namespace Microsoft::WRL;
using namespace DirectX;

class Renderer {
public:
    Renderer(UINT width, UINT height, std::wstring name);
    ~Renderer();

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }

    void OnInit();
    void OnUpdate();
    void OnRender();
    void OnDestroy();

private:
    void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = false);

    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    std::wstring m_title;

    static const UINT FrameCount = 2;
    static const UINT TextureWidth = 256;
    static const UINT TextureHeight = 256;
    static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.

    struct Vertex
    {
        XMFLOAT3 position;
        //XMFLOAT4 color;
        XMFLOAT2 uv;
    };

    // Adapter info
    bool m_useWarpDevice = false;

    // Pipeline objects
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_renderTargets[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    ComPtr<ID3D12DescriptorHeap> m_srvHeap;
    ComPtr<ID3D12PipelineState> m_pipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    UINT m_rtvDescriptorSize;

    // App resources
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_texture;

    // Synchronization objects
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForPreviousFrame();

    void DowngradeRootParameters(D3D12_ROOT_PARAMETER1* src, UINT numParameters, D3D12_ROOT_PARAMETER* dst,
        std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges, UINT& offset);
    void DowngradeDescriptorRanges(const D3D12_DESCRIPTOR_RANGE1* src, UINT NumDescriptorRanges,
        std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges);
    void DowngradeRootDescriptor(D3D12_ROOT_DESCRIPTOR1* src, D3D12_ROOT_DESCRIPTOR* dst);
};