#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6

#include <vector>
#include <memory>
#include <array>
#include <cstddef>

#include "ConstantData.h"
#include "ConstantBuffer.h"
#include "DescriptorAllocation.h"
#include "SharedConfig.h"

using Microsoft::WRL::ComPtr;

// aliasing
using MaterialCB = ConstantBuffer<MaterialConstantData>;
using LightCB = ConstantBuffer<LightConstantData>;
using CameraCB = ConstantBuffer<CameraConstantData>;
using ShadowCB = ConstantBuffer<ShadowConstantData>;

// Dynamic Data for each frame
class FrameResource
{
public:
    FrameResource(
        ID3D12Device10* pDevice,
        IDXGISwapChain* pSwapChain,
        UINT frameIndex,
        ResourceLayoutTracker& layoutTracker,
        DescriptorAllocation&& rtvAllocation,
        DescriptorAllocation&& gBufferRTVAllocation,
        DescriptorAllocation&& gBufferSRVAllocation);
    ~FrameResource();

    void ResetInstanceOffsetByte();
    void EnsureInstanceCapacity(UINT requiredSize);
    void CreateGBuffers(UINT64 width, UINT height, ResourceLayoutTracker& layoutTracker);
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
    UINT GetCameraConstantBufferCount() const;
    UINT GetMaterialConstantBufferCount() const;
    void AddCameraConstantBuffer();
    void CreateShadowConstantBuffer();
    void AddMaterialConstantBuffer(DescriptorAllocation&& allocation);
    void AddLightConstantBuffer(DescriptorAllocation&& allocation);
    D3D12_GPU_VIRTUAL_ADDRESS GetCameraCBVirtualAddress(UINT idx) const;
    D3D12_GPU_VIRTUAL_ADDRESS GetShadowCBVirtualAddress() const;
    DescriptorAllocation& GetMaterialCBVAllocationRef(UINT idx);
    DescriptorAllocation& GetLightCBVAllocationRef(UINT idx);
    void UpdateCameraConstantBuffer(UINT idx, CameraConstantData* pData);
    void UpdateShadowConstantBuffer(ShadowConstantData* pData);
    void UpdateMaterialConstantBuffer(UINT idx, MaterialConstantData* pData);
    void UpdateLightConstantBuffer(UINT idx, LightConstantData* pData);

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

    std::vector<std::unique_ptr<CameraCB>> m_cameraConstantBuffers;
    std::vector<std::unique_ptr<MaterialCB>> m_materialConstantBuffers;
    std::vector<std::unique_ptr<LightCB>> m_lightConstantBuffers;
    std::unique_ptr<ShadowCB> m_shadowConstantBuffer;

    ComPtr<ID3D12Resource> m_instanceUploadBuffer;
    UINT8* m_instanceBufferBegin = nullptr;
    UINT m_instanceCapacity = 1024;
    UINT m_instanceOffsetByte = 0;

    UINT64 m_fenceValue = 0;

    ID3D12Device10* m_pDevice;
};