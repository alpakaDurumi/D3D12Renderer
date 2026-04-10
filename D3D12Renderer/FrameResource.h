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
        DescriptorAllocation&& gBufferRTVAllocation,
        DescriptorAllocation&& gBufferSRVAllocation);
    ~FrameResource();

    void ResetInstanceOffsetByte();
    void EnsureInstanceCapacity(UINT requiredSize);
    void CreateGBuffers(UINT64 width, UINT height);
    void AcquireBackBuffer(IDXGISwapChain* pSwapChain, UINT frameIndex);

    // back buffers
    ID3D12Resource* GetRenderTarget() const;
    ID3D12Resource* GetGBuffer(GBufferSlot slot) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetGBufferRTVHandle(GBufferSlot slot) const;
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> GetGBufferRTVHandles() const;
    DescriptorAllocation& GetGBufferSRVAllocationRef();
    UINT64 GetFenceValue() const;

    // CB
    UploadAllocation PushConstantData(void* src, std::size_t size);

    // instance data
    void PushInstanceData(std::vector<InstanceData>& data);
    D3D12_GPU_VIRTUAL_ADDRESS GetInstanceBufferVirtualAddress() const;

    void SetFenceValue(UINT64 fenceValue);

    void ResetRenderTarget();
    void ResetGBuffer(GBufferSlot slot);

    static DXGI_FORMAT GetGBufferFormat(GBufferSlot slot);

private:
    ComPtr<ID3D12Resource> m_renderTarget;
    DescriptorAllocation m_rtvAllocation;

    std::array<ComPtr<ID3D12Resource>, static_cast<std::size_t>(GBufferSlot::NUM_GBUFFER_SLOTS)> m_gBuffers;
    DescriptorAllocation m_gBufferRTVAllocation;
    DescriptorAllocation m_gBufferSRVAllocation;

    TransientUploadAllocator m_uploadAllocator;

    ComPtr<ID3D12Resource> m_instanceUploadBuffer;
    UINT8* m_instanceBufferBegin = nullptr;
    UINT m_instanceCapacity = 1024;
    UINT m_instanceOffsetByte = 0;

    UINT64 m_fenceValue = 0;

    ID3D12Device10* m_pDevice;
};