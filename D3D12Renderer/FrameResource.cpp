#include "pch.h"
#include "FrameResource.h"

#include "D3DHelper.h"
#include "Utility.h"

FrameResource::FrameResource(
    ID3D12Device10* pDevice,
    IDXGISwapChain* pSwapChain,
    UINT frameIndex,
    DescriptorAllocation&& rtvAllocation,
    DescriptorAllocation&& sceneBufferRTVAllocation,
    DescriptorAllocation&& sceneBufferSRVAllocation,
    DescriptorAllocation&& gBufferRTVAllocation,
    DescriptorAllocation&& gBufferSRVAllocation,
    DescriptorAllocation&& selectionMaskRTVAllocation,
    DescriptorAllocation&& selectionMaskSRVAllocation,
    DescriptorAllocation&& horizontalDilatedMaskRTVAllocation,
    DescriptorAllocation&& horizontalDilatedMaskSRVAllocation)
    : m_pDevice(pDevice),
    m_backBufferRTVAllocation(std::move(rtvAllocation)),
    m_sceneColorBufferRTVAllocation(std::move(sceneBufferRTVAllocation)),
    m_sceneColorBufferSRVAllocation(std::move(sceneBufferSRVAllocation)),
    m_gBufferRTVAllocation(std::move(gBufferRTVAllocation)),
    m_gBufferSRVAllocation(std::move(gBufferSRVAllocation)),
    m_selectionMaskRTVAllocation(std::move(selectionMaskRTVAllocation)),
    m_selectionMaskSRVAllocation(std::move(selectionMaskSRVAllocation)),
    m_horizontalDilatedMaskRTVAllocation(std::move(horizontalDilatedMaskRTVAllocation)),
    m_horizontalDilatedMaskSRVAllocation(std::move(horizontalDilatedMaskSRVAllocation)),
    m_uploadAllocator(pDevice)
{
    assert(!m_backBufferRTVAllocation.IsNull() &&
        m_sceneColorBufferRTVAllocation.GetNumHandles() == SceneColorBufferCount &&
        m_sceneColorBufferSRVAllocation.GetNumHandles() == SceneColorBufferCount &&
        m_gBufferRTVAllocation.GetNumHandles() == static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS) &&
        m_gBufferSRVAllocation.GetNumHandles() == static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS) &&
        !m_selectionMaskRTVAllocation.IsNull(),
        !m_selectionMaskSRVAllocation.IsNull(),
        !m_horizontalDilatedMaskRTVAllocation.IsNull(),
        !m_horizontalDilatedMaskSRVAllocation.IsNull());

    // Back buffer
    AcquireBackBuffer(pSwapChain, frameIndex);
    D3DHelper::CreateRTV(m_pDevice, m_backBuffer.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, m_backBufferRTVAllocation.GetDescriptorHandle());

    // Create GBuffers and SceneColorBuffers, and masks
    auto rtDesc = m_backBuffer->GetDesc();
    const UINT64 width = rtDesc.Width;
    const UINT height = rtDesc.Height;

    CreateGBuffers(width, height);
    CreateSceneColorBuffers(width, height);
    CreateMasks(width, height);

    // Create Upload buffer
    D3DHelper::CreateUploadBuffer(m_pDevice, sizeof(InstanceData) * m_instanceCapacity, m_instanceUploadBuffer);
    D3D12_RANGE readRange = { 0, 0 };
    D3DHelper::ThrowIfFailed(m_instanceUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_instanceBufferBegin)));
}

FrameResource::~FrameResource()
{
    m_instanceUploadBuffer->Unmap(0, nullptr);
}

// Back buffer
void FrameResource::AcquireBackBuffer(IDXGISwapChain* pSwapChain, UINT frameIndex)
{
    D3DHelper::ThrowIfFailed(pSwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_backBuffer)));
}

ID3D12Resource* FrameResource::GetBackBuffer() const
{
    return m_backBuffer.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetBackBufferRTVHandle() const
{
    return m_backBufferRTVAllocation.GetDescriptorHandle();
}

void FrameResource::ResetBackBuffer()
{
    m_backBuffer.Reset();
}

// Scene color buffer
void FrameResource::CreateSceneColorBuffers(UINT64 width, UINT height)
{
    for (UINT i = 0; i < SceneColorBufferCount; ++i)
    {
        auto format = DXGI_FORMAT_R8G8B8A8_UNORM;

        DirectX::XMVECTORF32 color;
        color.v = DirectX::XMColorSRGBToRGB(DirectX::XMVectorSet(0.5f, 0.5f, 0.5f, 1.0f));
        auto clearValue = D3DHelper::CreateClearValue(format, color.f[0], color.f[1], color.f[2], color.f[3]);

        D3DHelper::CreateRenderTarget(m_pDevice, width, height, format, format, 1, m_sceneColorBuffers[i], &clearValue);

        D3DHelper::CreateRTV(m_pDevice, m_sceneColorBuffers[i].Get(), format, m_sceneColorBufferRTVAllocation.GetDescriptorHandle(i));
        D3DHelper::CreateSRV(m_pDevice, m_sceneColorBuffers[i].Get(), format, m_sceneColorBufferSRVAllocation.GetDescriptorHandle(i));
    }
}

ID3D12Resource* FrameResource::GetSceneColorBuffer(UINT index) const
{
    return m_sceneColorBuffers[index].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetSceneColorBufferRTVHandle(UINT index) const
{
    return m_sceneColorBufferRTVAllocation.GetDescriptorHandle(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetSceneColorBufferSRVHandle(UINT index) const
{
    return m_sceneColorBufferSRVAllocation.GetDescriptorHandle(index);
}

void FrameResource::ResetSceneColorBuffers()
{
    for (UINT i = 0; i < SceneColorBufferCount; ++i)
        m_sceneColorBuffers[i].Reset();
}

// GBuffer
void FrameResource::CreateGBuffers(UINT64 width, UINT height)
{
    for (UINT i = 0; i < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++i)
    {
        auto format = GetGBufferFormat(static_cast<GBufferSlot>(i));

        D3DHelper::CreateRenderTarget(m_pDevice, width, height, format, format, 1, m_gBuffers[i]);

        D3DHelper::CreateRTV(m_pDevice, m_gBuffers[i].Get(), format, m_gBufferRTVAllocation.GetDescriptorHandle(i));
        D3DHelper::CreateSRV(m_pDevice, m_gBuffers[i].Get(), format, m_gBufferSRVAllocation.GetDescriptorHandle(i));
    }
}

ID3D12Resource* FrameResource::GetGBuffer(GBufferSlot slot) const
{
    return m_gBuffers[static_cast<UINT>(slot)].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetGBufferRTVHandle() const
{
    return m_gBufferRTVAllocation.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetGBufferSRVHandle() const
{
    return m_gBufferSRVAllocation.GetDescriptorHandle();
}

DXGI_FORMAT FrameResource::GetGBufferFormat(GBufferSlot slot)
{
    switch (slot)
    {
    case GBufferSlot::ALBEDO:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case GBufferSlot::NORMAL:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case GBufferSlot::MATERIAL_AMBIENT:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case GBufferSlot::MATERIAL_SPECULAR:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:
        assert(false);
        return DXGI_FORMAT_UNKNOWN;
    }
}

void FrameResource::ResetGBuffers()
{
    for (auto& gBuffer : m_gBuffers)
        gBuffer.Reset();
}

// Masks
void FrameResource::CreateMasks(UINT64 width, UINT height)
{
    const auto format = DXGI_FORMAT_R8_UNORM;

    D3DHelper::CreateRenderTarget(m_pDevice, width, height, format, format, 1, m_selectionMask);
    D3DHelper::CreateRTV(m_pDevice, m_selectionMask.Get(), format, m_selectionMaskRTVAllocation.GetDescriptorHandle());
    D3DHelper::CreateSRV(m_pDevice, m_selectionMask.Get(), format, m_selectionMaskSRVAllocation.GetDescriptorHandle());

    D3DHelper::CreateRenderTarget(m_pDevice, width, height, format, format, 1, m_horizontalDilatedMask);
    D3DHelper::CreateRTV(m_pDevice, m_horizontalDilatedMask.Get(), format, m_horizontalDilatedMaskRTVAllocation.GetDescriptorHandle());
    D3DHelper::CreateSRV(m_pDevice, m_horizontalDilatedMask.Get(), format, m_horizontalDilatedMaskSRVAllocation.GetDescriptorHandle());
}

ID3D12Resource* FrameResource::GetSelectionMask() const
{
    return m_selectionMask.Get();
}

ID3D12Resource* FrameResource::GetHorizontalDilatedMask() const
{
    return m_horizontalDilatedMask.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetSelectionMaskRTVHandle() const
{
    return m_selectionMaskRTVAllocation.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetSelectionMaskSRVHandle() const
{
    return m_selectionMaskSRVAllocation.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetHorizontalDilatedMaskRTVHandle() const
{
    return m_horizontalDilatedMaskRTVAllocation.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetHorizontalDilatedMaskSRVHandle() const
{
    return m_horizontalDilatedMaskSRVAllocation.GetDescriptorHandle();
}

void FrameResource::ResetMasks()
{
    m_selectionMask.Reset();
    m_horizontalDilatedMask.Reset();
}

// Instance data
void FrameResource::ResetInstanceOffsetByte()
{
    m_instanceOffsetByte = 0;
}

void FrameResource::EnsureInstanceCapacity(UINT requiredSize)
{
    if (m_instanceCapacity < requiredSize)
    {
        m_instanceUploadBuffer->Unmap(0, nullptr);

        m_instanceCapacity = Utility::CeilPowerOfTwo(requiredSize);

        D3DHelper::CreateUploadBuffer(m_pDevice, sizeof(InstanceData) * m_instanceCapacity, m_instanceUploadBuffer);
        D3D12_RANGE readRange = { 0, 0 };
        D3DHelper::ThrowIfFailed(m_instanceUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_instanceBufferBegin)));
    }
}

void FrameResource::PushInstanceData(std::vector<InstanceData>& data)
{
    memcpy(m_instanceBufferBegin + m_instanceOffsetByte, data.data(), sizeof(InstanceData) * data.size());
    m_instanceOffsetByte += sizeof(InstanceData) * static_cast<UINT>(data.size());
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResource::GetInstanceBufferVirtualAddress() const
{
    return m_instanceUploadBuffer->GetGPUVirtualAddress();
}

// Transient upload
UploadAllocation FrameResource::PushConstantData(void* src, std::size_t size)
{
    return m_uploadAllocator.Push(src, size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
}

void FrameResource::ResetUploadAllocator()
{
    m_uploadAllocator.Reset();
}

// Synchronization
UINT64 FrameResource::GetFenceValue() const
{
    return m_fenceValue;
}

void FrameResource::SetFenceValue(UINT64 fenceValue)
{
    m_fenceValue = fenceValue;
}