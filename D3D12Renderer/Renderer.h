#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6
#include <DirectXCollision.h>

#include <string>
#include <vector>
#include <memory>
#include <array>

#include "Camera.h"
#include "InputManager.h"
#include "ConstantData.h"
#include "CommandQueue.h"
#include "CommandList.h"
#include "FrameResource.h"
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

    std::unique_ptr<DynamicDescriptorHeap> m_dynamicDescriptorHeapForCBVSRVUAV;
    ComPtr<ID3D12DescriptorHeap> m_samplerDescriptorHeap;
    std::unique_ptr<CommandQueue> m_commandQueue;
    std::unique_ptr<ResourceLayoutTracker> m_layoutTracker;
    std::unique_ptr<UploadBuffer> m_uploadBuffer;
    std::array<std::unique_ptr<DescriptorAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_descriptorAllocators;
    std::vector<std::unique_ptr<FrameResource>> m_frameResources;

    std::unique_ptr<RootSignature> m_rootSignature;
    std::unordered_map<PSOKey, ComPtr<ID3D12PipelineState>> m_pipelineStates;

    PSOKey m_currentPSOKey = { MeshType::DEFAULT, PassType::DEFAULT };

    std::unordered_map<MeshType, std::vector<D3D12_INPUT_ELEMENT_DESC>> m_inputLayouts;
    std::unordered_map<ShaderKey, ComPtr<ID3DBlob>> m_shaderBlobs;

    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    DescriptorAllocation m_dsvAllocation;

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
    std::vector<std::unique_ptr<Light>> m_lights;
    D3D12_VIEWPORT m_shadowMapViewport;
    D3D12_RECT m_shadowMapScissorRect;
    UINT m_shadowMapResolution = 2048;
    ShadowConstantData m_shadowConstantData;

    // WIP : texture filtering is fixed for now.
    TextureFiltering m_currentTextureFiltering = TextureFiltering::ANISOTROPIC_X16;

    // For ImGui
    std::unique_ptr<ImGuiDescriptorAllocator> m_imguiDescriptorAllocator;
    static Renderer* sm_instance;

    // Synchronization objects
    UINT m_frameIndex;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList(CommandList& commandList);
    void BuildImGuiFrame();
    void WaitForGPU();
    void MoveToNextFrame();
    void InitImGui();

    void SetTextureFiltering(TextureFiltering filtering);
    void SetMeshType(MeshType meshType);
    UINT CalcSamplerIndex(TextureFiltering filtering, TextureAddressingMode addressingMode);

    void BindDescriptorTables(ID3D12GraphicsCommandList7* pCommandList);

    void CreateRootSignature();
    ID3D12PipelineState* GetPipelineState(const PSOKey& psoKey);
    ID3DBlob* GetShaderBlob(const ShaderKey& shaderKey);

    void PrintFPS();

    void FixedUpdate(double fixedDt);

    void PrepareConstantData(float alpha);
    std::vector<BoundingSphere> CalcCascadeSpheres();
    void PrepareDirectionalLight(DirectionalLight& light, const std::vector<BoundingSphere>& cascadeSpheres);
    void PreparePointLight(PointLight& light);
    void PrepareSpotLight(SpotLight& light);

    void UpdateConstantBuffers(FrameResource& frameResource);

    void UpdateCameraConstantBuffer(FrameResource& frameResource);
    void UpdateMaterialConstantBuffer(FrameResource& frameResource);
    void UpdateShadowConstantBuffer(FrameResource& frameResource);
};