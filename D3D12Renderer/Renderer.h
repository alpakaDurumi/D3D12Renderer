#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6
#include <dxgidebug.h>
#include <DirectXMath.h>

#include <string>
#include <vector>

#include "Camera.h"
#include "InputManager.h"
#include "Mesh.h"
#include "FrameResource.h"
#include "ConstantData.h"
#include "ConstantBuffer.h"
#include "DescriptorHeapManager.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class Renderer
{
public:
    Renderer(std::wstring name);

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }

    void SetWidth(UINT width) { m_width = width; }
    void SetHeight(UINT height) { m_height = height; }
    void SetWarp(bool value) { m_useWarpDevice = value; }
    void UpdateViewport();

    void OnInit();
    void OnUpdate();
    void OnRender();
    void OnDestroy();
    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);
    void OnMouseMove(int xPos, int yPos);
    void OnResize(UINT width, UINT height);

private:
    UINT m_width = 1280;
    UINT m_height = 720;

    std::wstring m_title;

    static const UINT FrameCount = 2;

    // Adapter info
    bool m_useWarpDevice = false;

    // Pipeline objects
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12Device> m_device;
    ComPtr<ID3D12Resource> m_depthStencil;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;      // For frame-independent job
    //ComPtr<ID3D12CommandAllocator> m_bundleAllocator;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12RootSignature> m_rootSignature;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    DescriptorHeapManager m_cbvSrvUavHeap;
    ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    ComPtr<ID3D12PipelineState> m_defaultPipelineState;
    ComPtr<ID3D12PipelineState> m_instancedPipelineState;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    //ComPtr<ID3D12GraphicsCommandList> m_bundle;
    UINT m_rtvDescriptorSize;

    const UINT MaxDynamicCbvCountPerFrame = 128;
    const UINT MaxStaticSrvCount = 64;

    // App resources
    Camera m_camera;
    InputManager m_inputManager;
    std::vector<Mesh*> m_meshes;
    std::vector<InstancedMesh*> m_instancedMeshes;
    LightConstantData m_lightConstantData;
    CameraConstantData m_cameraConstantData;
    ComPtr<ID3D12Resource> m_texture;

    std::vector<FrameResource*> m_frameResources;

    // Synchronization objects
    UINT m_frameIndex;
    HANDLE m_fenceEvent;
    ComPtr<ID3D12Fence> m_fence;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList();
    void WaitForGPU();
    void MoveToNextFrame();
};