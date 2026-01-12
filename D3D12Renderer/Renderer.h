#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6

#include <string>
#include <vector>
#include <memory>
#include <array>

#include "Camera.h"
#include "InputManager.h"
#include "ConstantData.h"
#include "CommandQueue.h"
#include "ResourceLayoutTracker.h"
#include "UploadBuffer.h"
#include "Mesh.h"
#include "DescriptorAllocator.h"
#include "Texture.h"
#include "DynamicDescriptorHeap.h"
#include "RootSignature.h"
#include "ImGuiDescriptorAllocator.h"
#include "CacheKeys.h"
#include "Light.h"

class FrameResource;
class CommandList;

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class Renderer
{
public:
    Renderer(std::wstring name);

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }
    static Renderer* GetInstance() { return sm_instance; }

    void SetWidth(UINT width) { m_width = width; }
    void SetHeight(UINT height) { m_height = height; }
    void SetWarp(bool value) { m_useWarpDevice = value; }
    void SetPix();
    void UpdateWidthHeight();
    void ToggleFullScreen();
    void SetFullScreen(bool fullScreen);

    void OnInit();
    void OnUpdate();
    void OnRender();
    void OnDestroy();
    void OnKeyDown(WPARAM key);
    void OnKeyUp(WPARAM key);
    void OnMouseMove(int xPos, int yPos);
    void OnResize(UINT width, UINT height);
    void OnPrepareImGui();

    static void ImGuiSrvDescriptorAllocate(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle);
    static void ImGuiSrvDescriptorFree(D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle);

private:
    // Window
    UINT m_width = 1920;
    UINT m_height = 1080;

    std::wstring m_title;

    bool m_vSync = false;
    bool m_tearingSupported = false;
    bool m_fullScreen = false;

    RECT m_windowRect;
    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    static const UINT FrameCount = 2;

    // Adapter info
    bool m_useWarpDevice = false;

    // Pipeline objects
    ComPtr<ID3D12Device10> m_device;
    ComPtr<IDXGISwapChain3> m_swapChain;

    std::unique_ptr<DynamicDescriptorHeap> m_dynamicDescriptorHeap;
    std::unique_ptr<CommandQueue> m_commandQueue;
    std::unique_ptr<ResourceLayoutTracker> m_layoutTracker;
    std::unique_ptr<UploadBuffer> m_uploadBuffer;
    std::array<std::unique_ptr<DescriptorAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_descriptorAllocators;
    std::vector<FrameResource*> m_frameResources;

    std::unordered_map<RSKey, std::unique_ptr<RootSignature>> m_rootSignatures;
    std::unordered_map<PSOKey, ComPtr<ID3D12PipelineState>> m_pipelineStates;

    RSKey m_currentRSKey = { TextureFiltering::ANISOTROPIC_X16, TextureAddressingMode::WRAP };
    PSOKey m_currentPSOKey = { TextureFiltering::ANISOTROPIC_X16, TextureAddressingMode::WRAP, MeshType::DEFUALT };

    std::unordered_map<MeshType, std::vector<D3D12_INPUT_ELEMENT_DESC>> m_inputLayouts;
    std::unordered_map<ShaderKey, ComPtr<ID3DBlob>> m_shaderBlobs;

    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    std::unique_ptr<DescriptorAllocation> m_dsvAllocation;

    // App resources
    // 
    // Main Camera
    Camera m_camera;
    UINT m_mainCameraIndex = 0;
    CameraConstantData m_cameraConstantData;

    InputManager m_inputManager;

    std::vector<Mesh> m_meshes;
    std::vector<InstancedMesh> m_instancedMeshes;

    MaterialConstantData m_materialConstantData;
    
    std::unique_ptr<Texture> m_albedo;
    std::unique_ptr<Texture> m_normalMap;
    std::unique_ptr<Texture> m_heightMap;

    // Lights, Shadows
    std::vector<Light> m_lights;
    D3D12_VIEWPORT m_shadowMapViewport;
    D3D12_RECT m_shadowMapScissorRect;
    UINT m_shadowMapResolution = 2048;
    ShadowConstantData m_shadowConstantData;

    // For ImGui
    std::unique_ptr<ImGuiDescriptorAllocator> m_imguiDescriptorAllocator;
    static Renderer* sm_instance;

    // Synchronization objects
    UINT m_frameIndex;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList(CommandList& commandList);
    void WaitForGPU();
    void MoveToNextFrame();
    void InitImGui();

    void SetTextureFiltering(TextureFiltering filtering);
    void SetTextureAddressingMode(TextureAddressingMode addressingMode);
    void SetMeshType(MeshType meshType);

    RootSignature* GetRootSignature(const RSKey& rsKey);
    ID3D12PipelineState* GetPipelineState(const PSOKey& psoKey);
    ID3DBlob* GetShaderBlob(const ShaderKey& shaderKey);

    void UpdateCameraConstantBuffer(FrameResource* pFrameResource);
    void UpdateMaterialConstantBuffer(FrameResource* pFrameResource);
};