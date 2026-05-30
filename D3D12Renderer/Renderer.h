#pragma once

#include <array>
#include <chrono>
#include <ratio>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <basetsd.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <minwindef.h>
#include <wrl/client.h>

#include "Aliases.h"
#include "CacheKeys.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "ConstantData.h"
#include "DescriptorAllocator.h"
#include "DynamicDescriptorHeap.h"
#include "FrameResource.h"
#include "ImGuiDescriptorAllocator.h"
#include "InputManager.h"
#include "RenderGraph.h"
#include "RendererConfig.h"
#include "RootSignature.h"
#include "SceneHandles.h"
#include "SceneManager.h"
#include "Texture.h"
#include "UploadAllocation.h"
#include "View.h"

struct GeometryData;
class DescriptorAllocation;
class DirectionalLight;
class PointLight;
class SpotLight;
class TransientUploadAllocator;

class Renderer
{
public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;

    Renderer(std::wstring name);
    ~Renderer();

    std::pair<UINT, UINT> GetResolution() const;
    const WCHAR* GetTitle() const;
    static Renderer* GetInstance();

    void SetWarp(bool value);
    void SetPix();
    void SetResolution(UINT width, UINT height);

    void Init(UINT dpi);
    void ProcessInput();
    void BuildImGuiFrame();
    void Update();
    void Render();
    void Destroy();

    void OnKeyDown(VKCode key);
    void OnKeyUp(VKCode key);
    void OnMouseButtonDown(UINT button);
    void OnMouseButtonUp(UINT button);
    void OnMouseMove(int x, int y, int cx, int cy);
    void OnMouseWheel(float stepDelta);
    void OnKillFocus();
    void OnResize(UINT width, UINT height);
    void OnDpiChanged(UINT dpi);

    static void ImGuiSrvDescriptorAllocate(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle);
    static void ImGuiSrvDescriptorFree(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

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

    // Adapter info
    bool m_useWarpDevice = false;

    // Pipeline objects
    Microsoft::WRL::ComPtr<ID3D12Device10> m_device;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;

    DynamicDescriptorHeap m_dynamicDescriptorHeapForCbvSrvUav;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_samplerDescriptorHeap;
    CommandQueue m_commandQueue;
    std::array<DescriptorAllocator, D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES> m_descriptorAllocators;
    std::array<FrameResource, FrameCount> m_frameResources;

    RootSignature m_rootSignature;
    std::unordered_map<PSOKey, Microsoft::WRL::ComPtr<ID3D12PipelineState>> m_pipelineStates;

    PSOKey m_currentPSOKey = {PassType::FORWARD_COLORING};

    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
    std::unordered_map<ShaderKey, std::vector<char>> m_shaderBlobs;

    Texture m_depthStencilBuffer;
    DepthStencilView m_dsv;
    DepthStencilView m_readOnlyDsv;
    ShaderResourceView m_depthSrv;

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

    inline static constexpr float DEFAULT_FOCUS_DIST = 30.0f;

    bool m_cameraControl = false;
    bool m_orbiting = false;
    DirectX::XMFLOAT3 m_orbitPivot;
    float m_orbitDistance = DEFAULT_FOCUS_DIST;
    bool m_panning = false;

    std::vector<EntityHandle> m_previewRotations;

    // Shadows
    D3D12_VIEWPORT m_shadowMapViewport;
    D3D12_RECT m_shadowMapScissorRect;
    UINT m_shadowMapResolution = 2048;
    ShadowConstantData m_shadowConstantData;
    UploadAllocation m_shadowUploadAllocation;

    TextureFiltering m_currentTextureFiltering = TextureFiltering::ANISOTROPIC_X16;

    // For ImGui
    ImGuiDescriptorAllocator m_imguiDescriptorAllocator;
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
    void WaitForGpu();
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

    AssetTextureHandle CreateAssetTexture(
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& allocation,
        TransientUploadAllocator& uploadAllocator,
        const std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height);

    AssetTextureHandle CreateAssetTexture(
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& allocation,
        TransientUploadAllocator& uploadAllocator,
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage,
        bool isCubeMap);

    void SetFpsCap(std::string fps);

    void BindDescriptorTables(ID3D12GraphicsCommandList* pCommandList);

    void CreateRootSignature();
    ID3D12PipelineState* GetPipelineState(const PSOKey& psoKey);
    const std::vector<char>& GetShaderBlobRef(const ShaderKey& shaderKey) const;

    void FixedUpdate(double fixedDt);

    void PrepareConstantData(float alpha);
    void PrepareTransform(Entity& entity, DirectX::XMMATRIX& accumulated, float alpha);
    std::vector<DirectX::BoundingSphere> CalcCascadeSpheres();
    void PrepareDirectionalLight(DirectionalLight& light, const std::vector<DirectX::BoundingSphere>& cascadeSpheres);
    void PreparePointLight(PointLight& light);
    void PrepareSpotLight(SpotLight& light);

    void UpdateConstantBuffers(FrameResource& frameResource);

    void DrawMesh(ID3D12GraphicsCommandList* pCommandList, MeshHandle meshhandle, PassType passType, D3D12_GPU_VIRTUAL_ADDRESS instanceBufferBase);
    void DrawEntity(ID3D12GraphicsCommandList* pCommandList, EntityHandle entityHandle, D3D12_GPU_VIRTUAL_ADDRESS instanceBufferBase);

    void BeginOrbit();

    void ToggleFullScreen();
    void SetFullScreen(bool fullScreen);
};
