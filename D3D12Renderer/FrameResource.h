#pragma once

#include <array>
#include <cstddef>
#include <vector>

#include <basetsd.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <minwindef.h>

#include "Buffer.h"
#include "ImGuiShaderResourceView.h"
#include "SharedConfig.h"
#include "Texture.h"
#include "TransientUploadAllocator.h"
#include "View.h"

class DescriptorAllocation;
class ImGuiDescriptorAllocation;
struct InstanceData;
struct UploadAllocation;

// Dynamic Data for each frame
class FrameResource
{
public:
    FrameResource(const FrameResource&) = delete;
    FrameResource& operator=(const FrameResource&) = delete;
    FrameResource(FrameResource&&) = delete;
    FrameResource& operator=(FrameResource&&) = delete;

    FrameResource() = default;
    ~FrameResource();

    void Init(
        ID3D12Device10* pDevice,
        IDXGISwapChain* pSwapChain,
        UINT frameIndex,
        DescriptorAllocation&& rtvAllocation,
        DescriptorAllocation&& sceneBufferRtvAllocation,
        DescriptorAllocation&& sceneBufferSrvAllocation,
        DescriptorAllocation&& gBufferRtvAllocation,
        DescriptorAllocation&& gBufferSrvAllocation,
        DescriptorAllocation&& selectionMaskRtvAllocation,
        DescriptorAllocation&& selectionMaskSrvAllocation,
        DescriptorAllocation&& horizontalDilatedMaskRtvAllocation,
        DescriptorAllocation&& horizontalDilatedMaskSrvAllocation,
        DescriptorAllocation&& toneMappedBufferRtvAllocation,
        ImGuiDescriptorAllocation&& toneMappedBufferSrvAllocation);

    // Back buffer
    void AcquireBackBuffer(IDXGISwapChain* pSwapChain, UINT frameIndex);
    ID3D12Resource* GetBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRtvHandle() const;
    void ResetBackBuffer();

    // Scene color buffer
    void CreateSceneColorBuffers(UINT64 width, UINT height);
    ID3D12Resource* GetSceneColorBuffer(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSceneColorBufferRtvHandle(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSceneColorBufferSrvHandle(UINT index) const;

    // GBuffer
    void CreateGBuffers(UINT64 width, UINT height);
    ID3D12Resource* GetGBuffer(GBufferSlot slot) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetGBufferBaseRtvHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetGBufferBaseSrvHandle() const;
    static DXGI_FORMAT GetGBufferFormat(GBufferSlot slot);

    // Masks
    void CreateMasks(UINT64 width, UINT height);
    ID3D12Resource* GetSelectionMask() const;
    ID3D12Resource* GetHorizontalDilatedMask() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSelectionMaskRtvHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSelectionMaskSrvHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetHorizontalDilatedMaskRtvHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetHorizontalDilatedMaskSrvHandle() const;

    // ToneMappedBuffer
    void CreateToneMappedBuffer(UINT64 width, UINT height);
    ID3D12Resource* GetToneMappedBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetToneMappedBufferRtvHandle() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetToneMappedBufferSrvHandle() const;

    // Instance data
    void ResetInstanceOffsetByte();
    void EnsureInstanceCapacity(UINT requiredSize);
    void PushInstanceData(std::vector<InstanceData>& data);
    D3D12_GPU_VIRTUAL_ADDRESS GetInstanceBufferVirtualAddress() const;

    // Transient upload
    UploadAllocation PushConstantData(void* src, std::size_t size);
    void ResetUploadAllocator();

    // Synchronization
    UINT64 GetSignaledFenceValue() const;
    void UpdateSignaledFenceValue(UINT64 signaledFenceValue);

    inline static constexpr UINT SceneColorBufferCount = 2;

private:
    Texture m_backBuffer;
    RenderTargetView m_backBufferRtv;

    std::array<Texture, SceneColorBufferCount> m_sceneColorBuffers;
    std::array<RenderTargetView, SceneColorBufferCount> m_sceneColorBufferRtvs;
    std::array<ShaderResourceView, SceneColorBufferCount> m_sceneColorBufferSrvs;

    std::array<Texture, static_cast<std::size_t>(GBufferSlot::NUM_GBUFFER_SLOTS)> m_gBuffers;
    std::array<RenderTargetView, static_cast<std::size_t>(GBufferSlot::NUM_GBUFFER_SLOTS)> m_gBufferRtvs;
    std::array<ShaderResourceView, static_cast<std::size_t>(GBufferSlot::NUM_GBUFFER_SLOTS)> m_gBufferSrvs;

    Texture m_selectionMask;
    RenderTargetView m_selectionMaskRtv;
    ShaderResourceView m_selectionMaskSrv;

    Texture m_horizontalDilatedMask;
    RenderTargetView m_horizontalDilatedMaskRtv;
    ShaderResourceView m_horizontalDilatedMaskSrv;

    Texture m_toneMappedBuffer;
    RenderTargetView m_toneMappedBufferRtv;
    ImGuiShaderResourceView m_toneMappedBufferSrv;

    Buffer m_instanceUploadBuffer;
    UINT8* m_instanceBufferBegin = nullptr;
    UINT m_instanceOffsetByte = 0;
    UINT m_instanceCapacity = 1024;

    TransientUploadAllocator m_uploadAllocator;

    UINT64 m_signaledFenceValue = 0;

    ID3D12Device10* m_pDevice = nullptr;
};
