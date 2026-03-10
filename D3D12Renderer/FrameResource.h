#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6

#include <vector>
#include <memory>
#include <array>

#include "ConstantData.h"
#include "ConstantBuffer.h"
#include "DescriptorAllocation.h"

using Microsoft::WRL::ComPtr;

enum class GBufferSlot
{
    ALBEDO,
    NORMAL,
    NUM_GBUFFER_SLOTS
};

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
    //void AllocateInstanceData();

    DXGI_FORMAT GetGBufferFormat(GBufferSlot slot);

//private:
    ComPtr<ID3D12Resource> m_renderTarget;
    DescriptorAllocation m_rtvAllocation;

    std::array<ComPtr<ID3D12Resource>, static_cast<std::size_t>(GBufferSlot::NUM_GBUFFER_SLOTS)> m_gBuffers;
    DescriptorAllocation m_gBufferRTVAllocation;
    DescriptorAllocation m_gBufferSRVAllocation;

    std::vector<std::unique_ptr<MaterialCB>> m_materialConstantBuffers;
    std::vector<std::unique_ptr<LightCB>> m_lightConstantBuffers;
    std::vector<std::unique_ptr<CameraCB>> m_cameraConstantBuffers;
    std::unique_ptr<ShadowCB> m_shadowConstantBuffer;

    ComPtr<ID3D12Resource> m_instanceUploadBuffer;
    UINT8* m_instanceBufferBegin = nullptr;
    UINT m_instanceCapacity = 1024;
    UINT m_instanceOffsetByte = 0;

    UINT64 m_fenceValue = 0;

    ID3D12Device10* m_pDevice;
};