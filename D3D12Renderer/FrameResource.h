#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6

#include <vector>
#include <array>
#include <cstddef>

#include "DescriptorAllocation.h"
#include "SharedConfig.h"
#include "TransientUploadAllocator.h"
#include "InstanceData.h"

using Microsoft::WRL::ComPtr;

// Dynamic Data for each frame
class FrameResource
{
public:
    FrameResource(
        ID3D12Device10* pDevice,
        IDXGISwapChain* pSwapChain,
        UINT frameIndex,
        DescriptorAllocation&& rtvAllocation,
        DescriptorAllocation&& sceneBufferRTVAllocation,
        DescriptorAllocation&& sceneBufferSRVAllocation,
        DescriptorAllocation&& gBufferRTVAllocation,
        DescriptorAllocation&& gBufferSRVAllocation);
    ~FrameResource();

    // Back buffer
    void AcquireBackBuffer(IDXGISwapChain* pSwapChain, UINT frameIndex);
    ID3D12Resource* GetBackBuffer() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetBackBufferRTVHandle() const;
    void ResetBackBuffer();

    // Scene color buffer
    void CreateSceneColorBuffers(UINT64 width, UINT height);
    ID3D12Resource* GetSceneColorBuffer(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSceneColorBufferRTVHandle(UINT index) const;
    DescriptorAllocation& GetSceneColorBufferSRVAllocationRef(UINT index);
    void ResetSceneColorBuffers();

    // GBuffer
    void CreateGBuffers(UINT64 width, UINT height);
    ID3D12Resource* GetGBuffer(GBufferSlot slot) const;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GetGBufferRTVHandles() const;
    DescriptorAllocation& GetGBufferSRVAllocationRef();
    static DXGI_FORMAT GetGBufferFormat(GBufferSlot slot);
    void ResetGBuffers();

    // Instance data
    void ResetInstanceOffsetByte();
    void EnsureInstanceCapacity(UINT requiredSize);
    void PushInstanceData(std::vector<InstanceData>& data);
    D3D12_GPU_VIRTUAL_ADDRESS GetInstanceBufferVirtualAddress() const;

    // Transient upload
    UploadAllocation PushConstantData(void* src, std::size_t size);
    void ResetUploadAllocator();

    // Synchronization
    UINT64 GetFenceValue() const;
    void SetFenceValue(UINT64 fenceValue);

    inline static constexpr UINT SceneColorBufferCount = 2;

private:
    ComPtr<ID3D12Resource> m_backBuffer;
    DescriptorAllocation m_backBufferRTVAllocation;
    
    std::array<ComPtr<ID3D12Resource>, SceneColorBufferCount> m_sceneColorBuffers;
    std::array<DescriptorAllocation, SceneColorBufferCount> m_sceneColorBufferRTVAllocations;
    std::array<DescriptorAllocation, SceneColorBufferCount> m_sceneColorBufferSRVAllocations;

    std::array<ComPtr<ID3D12Resource>, static_cast<std::size_t>(GBufferSlot::NUM_GBUFFER_SLOTS)> m_gBuffers;
    DescriptorAllocation m_gBufferRTVAllocation;
    DescriptorAllocation m_gBufferSRVAllocation;

    ComPtr<ID3D12Resource> m_instanceUploadBuffer;
    UINT8* m_instanceBufferBegin = nullptr;
    UINT m_instanceOffsetByte = 0;
    UINT m_instanceCapacity = 1024;

    TransientUploadAllocator m_uploadAllocator;

    UINT64 m_fenceValue = 0;

    ID3D12Device10* m_pDevice;
};