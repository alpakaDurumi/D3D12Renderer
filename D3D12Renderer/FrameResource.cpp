#include "pch.h"
#include "FrameResource.h"

#include "D3DHelper.h"
#include "Utility.h"

using namespace D3DHelper;

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
    m_backBufferRtv(std::move(rtvAllocation)),
    m_selectionMaskRtv(std::move(selectionMaskRTVAllocation)),
    m_selectionMaskSrv(std::move(selectionMaskSRVAllocation)),
    m_horizontalDilatedMaskRtv(std::move(horizontalDilatedMaskRTVAllocation)),
    m_horizontalDilatedMaskSrv(std::move(horizontalDilatedMaskSRVAllocation)),
    m_uploadAllocator(pDevice)
{
    // Back buffer
    AcquireBackBuffer(pSwapChain, frameIndex);
    InitBackBufferRtv();

    auto rtDesc = m_backBuffer.Get()->GetDesc();
    const UINT64 width = rtDesc.Width;
    const UINT height = rtDesc.Height;

    // Scene color buffers
    {
        auto rtvAllocs = sceneBufferRTVAllocation.Split();
        auto srvAllocs = sceneBufferSRVAllocation.Split();
        for (UINT i = 0; i < SceneColorBufferCount; ++i)
        {
            m_sceneColorBufferRtvs[i] = RenderTargetView(std::move(rtvAllocs[i]));
            m_sceneColorBufferSrvs[i] = ShaderResourceView(std::move(srvAllocs[i]));
        }
        CreateSceneColorBuffers(width, height);
    }

    // GBuffers
    {
        auto rtvAllocs = gBufferRTVAllocation.Split();
        auto srvAllocs = gBufferSRVAllocation.Split();
        for (UINT i = 0; i < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++i)
        {
            m_gBufferRtvs[i] = RenderTargetView(std::move(rtvAllocs[i]));
            m_gBufferSrvs[i] = ShaderResourceView(std::move(srvAllocs[i]));
        }
        CreateGBuffers(width, height);
    }

    // Create masks
    CreateMasks(width, height);

    // Create Upload buffer
    m_instanceUploadBuffer = Buffer(m_pDevice, sizeof(InstanceData) * m_instanceCapacity, D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RANGE readRange = { 0, 0 };
    ThrowIfFailed(m_instanceUploadBuffer.Get()->Map(0, &readRange, reinterpret_cast<void**>(&m_instanceBufferBegin)));
}

FrameResource::~FrameResource()
{
    m_instanceUploadBuffer.Get()->Unmap(0, nullptr);
}

// Back buffer
void FrameResource::AcquireBackBuffer(IDXGISwapChain* pSwapChain, UINT frameIndex)
{
    ComPtr<ID3D12Resource> backBuffer;
    ThrowIfFailed(pSwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&backBuffer)));
    m_backBuffer = Texture(std::move(backBuffer));
}

ID3D12Resource* FrameResource::GetBackBuffer() const
{
    return m_backBuffer.Get();
}

void FrameResource::InitBackBufferRtv()
{
    m_backBufferRtv.Init(m_pDevice, m_backBuffer.Get(), GetRtvDesc(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0));
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetBackBufferRtvHandle() const
{
    return m_backBufferRtv.GetHandle();
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
        const auto format = DXGI_FORMAT_R8G8B8A8_UNORM;

        DirectX::XMVECTORF32 color;
        color.v = DirectX::XMColorSRGBToRGB(DirectX::XMVectorSet(0.5f, 0.5f, 0.5f, 1.0f));
        const auto clearValue = CreateClearValue(format, color.f[0], color.f[1], color.f[2], color.f[3]);

        m_sceneColorBuffers[i] = Texture(
            m_pDevice,
            GetTexture2DDesc(width, height, 1, 1, format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
            D3D12_BARRIER_LAYOUT_RENDER_TARGET,
            &clearValue);

        m_sceneColorBufferRtvs[i].Init(m_pDevice, m_sceneColorBuffers[i].Get(), GetRtvDesc(format, 0));
        m_sceneColorBufferSrvs[i].Init(m_pDevice, m_sceneColorBuffers[i].Get(), GetSrvDesc(format, 1));
    }
}

ID3D12Resource* FrameResource::GetSceneColorBuffer(UINT index) const
{
    return m_sceneColorBuffers[index].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetSceneColorBufferRtvHandle(UINT index) const
{
    return m_sceneColorBufferRtvs[index].GetHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetSceneColorBufferSrvHandle(UINT index) const
{
    return m_sceneColorBufferSrvs[index].GetHandle();
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
        const auto format = GetGBufferFormat(static_cast<GBufferSlot>(i));
        const auto clearValue = CreateClearValue(format, 0.0f, 0.0f, 0.0f, 0.0f);

        m_gBuffers[i] = Texture(
            m_pDevice,
            GetTexture2DDesc(width, height, 1, 1, format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
            D3D12_BARRIER_LAYOUT_RENDER_TARGET,
            &clearValue);

        m_gBufferRtvs[i].Init(m_pDevice, m_gBuffers[i].Get(), GetRtvDesc(format, 0));
        m_gBufferSrvs[i].Init(m_pDevice, m_gBuffers[i].Get(), GetSrvDesc(format, 1));
    }
}

ID3D12Resource* FrameResource::GetGBuffer(GBufferSlot slot) const
{
    return m_gBuffers[static_cast<UINT>(slot)].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetGBufferBaseRtvHandle() const
{
    return m_gBufferRtvs[0].GetHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetGBufferBaseSrvHandle() const
{
    return m_gBufferSrvs[0].GetHandle();
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
    const auto clearValue = CreateClearValue(format, 0.0f, 0.0f, 0.0f, 0.0f);

    m_selectionMask = Texture(
        m_pDevice,
        GetTexture2DDesc(width, height, 1, 1, format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        &clearValue);

    m_selectionMaskRtv.Init(m_pDevice, m_selectionMask.Get(), GetRtvDesc(format, 0));
    m_selectionMaskSrv.Init(m_pDevice, m_selectionMask.Get(), GetSrvDesc(format, 1));

    m_horizontalDilatedMask = Texture(
        m_pDevice,
        GetTexture2DDesc(width, height, 1, 1, format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        &clearValue);

    m_horizontalDilatedMaskRtv.Init(m_pDevice, m_horizontalDilatedMask.Get(), GetRtvDesc(format, 0));
    m_horizontalDilatedMaskSrv.Init(m_pDevice, m_horizontalDilatedMask.Get(), GetSrvDesc(format, 1));
}

ID3D12Resource* FrameResource::GetSelectionMask() const
{
    return m_selectionMask.Get();
}

ID3D12Resource* FrameResource::GetHorizontalDilatedMask() const
{
    return m_horizontalDilatedMask.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetSelectionMaskRtvHandle() const
{
    return m_selectionMaskRtv.GetHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetSelectionMaskSrvHandle() const
{
    return m_selectionMaskSrv.GetHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetHorizontalDilatedMaskRtvHandle() const
{
    return m_horizontalDilatedMaskRtv.GetHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetHorizontalDilatedMaskSrvHandle() const
{
    return m_horizontalDilatedMaskSrv.GetHandle();
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
        m_instanceUploadBuffer.Get()->Unmap(0, nullptr);

        m_instanceCapacity = Utility::CeilPowerOfTwo(requiredSize);

        m_instanceUploadBuffer = Buffer(m_pDevice, sizeof(InstanceData) * m_instanceCapacity, D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RANGE readRange = { 0, 0 };
        ThrowIfFailed(m_instanceUploadBuffer.Get()->Map(0, &readRange, reinterpret_cast<void**>(&m_instanceBufferBegin)));
    }
}

void FrameResource::PushInstanceData(std::vector<InstanceData>& data)
{
    memcpy(m_instanceBufferBegin + m_instanceOffsetByte, data.data(), sizeof(InstanceData) * data.size());
    m_instanceOffsetByte += sizeof(InstanceData) * static_cast<UINT>(data.size());
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResource::GetInstanceBufferVirtualAddress() const
{
    return m_instanceUploadBuffer.Get()->GetGPUVirtualAddress();
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