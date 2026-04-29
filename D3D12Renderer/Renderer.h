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
#include <chrono>
#include <unordered_map>
#include <cstddef>

#include "Camera.h"
#include "InputManager.h"
#include "ConstantData.h"
#include "CommandQueue.h"
#include "FrameResource.h"
#include "DescriptorAllocator.h"
#include "DynamicDescriptorHeap.h"
#include "RootSignature.h"
#include "ImGuiDescriptorAllocator.h"
#include "CacheKeys.h"
#include "Light.h"
#include "SharedConfig.h"
#include "RenderGraph.h"
#include "TransientUploadAllocator.h"
#include "Aliases.h"
#include "SceneManager.h"

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

    void OnInit(UINT dpi);
    void OnUpdate();
    void OnRender();
    void OnDestroy();
    void OnKeyDown(VKCode key);
    void OnKeyUp(VKCode key);
    void OnMouseButtonDown(UINT button);
    void OnMouseButtonUp(UINT button);
    void OnMouseMove(int x, int y, int cx, int cy);
    void OnMouseWheel(float stepDelta);
    void OnKillFocus();
    void OnResize(UINT width, UINT height);
    void OnDpiChanged(UINT dpi);

    void BuildImGuiFrame();

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
    int m_fpsCap = -1;

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
    std::array<std::unique_ptr<DescriptorAllocator>, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_descriptorAllocators;
    std::vector<std::unique_ptr<FrameResource>> m_frameResources;

    std::unique_ptr<RootSignature> m_rootSignature;
    std::unordered_map<PSOKey, ComPtr<ID3D12PipelineState>> m_pipelineStates;

    PSOKey m_currentPSOKey = { PassType::FORWARD_COLORING };

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
    std::unordered_map<ShaderKey, std::vector<char>> m_shaderBlobs;

    ComPtr<ID3D12Resource> m_depthStencilBuffer;
    DescriptorAllocation m_dsvAllocation;
    DescriptorAllocation m_readOnlyDSVAllocation;
    DescriptorAllocation m_depthSRVAllocation;

    // App resources
    // 
    // Main Camera
    Camera m_camera;
    CameraConstantData m_cameraConstantData;
    UploadAllocation m_cameraUploadAllocation;

    InputManager m_inputManager;

    RenderGraph m_renderGraph;

    SceneManager m_sceneManager;
    EntityHandle m_selected;
    XMFLOAT3 m_orbitPivot;
    float m_orbitDistance;
    bool m_orbiting = false;
    bool m_cameraControl = false;

    std::vector<EntityHandle> m_previewRotations;

    // Shadows
    D3D12_VIEWPORT m_shadowMapViewport;
    D3D12_RECT m_shadowMapScissorRect;
    UINT m_shadowMapResolution = 2048;
    ShadowConstantData m_shadowConstantData;
    UploadAllocation m_shadowUploadAllocation;

    TextureFiltering m_currentTextureFiltering = TextureFiltering::ANISOTROPIC_X16;

    // For ImGui
    std::unique_ptr<ImGuiDescriptorAllocator> m_imguiDescriptorAllocator;
    static Renderer* sm_instance;

    std::chrono::steady_clock m_clock;
    std::chrono::time_point<std::chrono::steady_clock> m_prevTime;
    std::chrono::time_point<std::chrono::steady_clock> m_deadLine;
    std::chrono::duration<double, std::milli> m_deltaTime;

    float m_dpiScale;

    // Synchronization objects
    UINT m_frameIndex;

    void LoadPipeline();
    void LoadAssets();
    void PopulateCommandList(ID3D12GraphicsCommandList7* pCommandList);
    void WaitForGPU();
    void MoveToNextFrame();

    void InitImGui();
    void RenderEntityNode(const Entity& entity, EntityHandle& selected, EntityHandle& toDelete, bool& selectionChanged);

    void PrepareRenderGraph();
    void ApplyPassBarriers(RenderGraph& renderGraph, PassType passType, ID3D12GraphicsCommandList7* pCommandList);

    void SetTextureFiltering(TextureFiltering filtering);

    MaterialHandle CreateMaterial();
    MaterialHandle CreateMaterial(const AssetID& id);
    MaterialHandle CloneMaterial(MaterialHandle src);

    MeshHandle CreateMesh(ID3D12GraphicsCommandList7* pCommandList, TransientUploadAllocator& allocator, const GeometryData& data);

    DirectionalLightHandle CreateDirectionalLight();
    PointLightHandle CreatePointLight();
    SpotLightHandle CreateSpotLight();

    TextureHandle CreateTexture(
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& allocation,
        TransientUploadAllocator& uploadAllocator,
        const std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height);

    TextureHandle CreateTexture(
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& allocation,
        TransientUploadAllocator& uploadAllocator,
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage,
        bool isCubeMap);

    void SetFpsCap(std::string fps);

    void BindDescriptorTables(ID3D12GraphicsCommandList7* pCommandList);

    void CreateRootSignature();
    ID3D12PipelineState* GetPipelineState(const PSOKey& psoKey);
    const std::vector<char>& GetShaderBlobRef(const ShaderKey& shaderKey) const;

    void FixedUpdate(double fixedDt);

    void PrepareConstantData(float alpha);
    void PrepareTransform(Entity& entity, XMMATRIX& accumulated, float alpha);
    std::vector<BoundingSphere> CalcCascadeSpheres();
    void PrepareDirectionalLight(DirectionalLight& light, const std::vector<BoundingSphere>& cascadeSpheres);
    void PreparePointLight(PointLight& light);
    void PrepareSpotLight(SpotLight& light);

    void UpdateConstantBuffers(FrameResource& frameResource);

    void DrawMesh(ID3D12GraphicsCommandList7* pCommandList, MeshHandle meshhandle, PassType passType, D3D12_GPU_VIRTUAL_ADDRESS instanceBufferBase);

    void ProcessInput();
    void BeginOrbit();
};