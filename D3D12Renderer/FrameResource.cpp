#include "pch.h"

#include "FrameResource.h"

#include "D3DHelper.h"
#include "DescriptorAllocation.h"
#include "InstanceData.h"
#include "UploadAllocation.h"
#include "Utility.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

FrameResource::~FrameResource()
{
    if (m_instanceBufferBegin)
        m_instanceUploadBuffer.Get()->Unmap(0, nullptr);
}

void FrameResource::Init(
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
    ImGuiDescriptorAllocation&& toneMappedBufferSrvAllocation)
{
    m_pDevice = pDevice;

    // Back buffer
    {
        m_backBufferRtv = RenderTargetView(std::move(rtvAllocation));
        AcquireBackBuffer(pSwapChain, frameIndex);
    }

    auto rtDesc = m_backBuffer.Get()->GetDesc();
    const UINT64 width = rtDesc.Width;
    const UINT height = rtDesc.Height;

    // Scene color buffers
    {
        auto rtvAllocs = sceneBufferRtvAllocation.Split();
        auto srvAllocs = sceneBufferSrvAllocation.Split();
        for (UINT i = 0; i < SceneColorBufferCount; ++i)
        {
            m_sceneColorBufferRtvs[i] = RenderTargetView(std::move(rtvAllocs[i]));
            m_sceneColorBufferSrvs[i] = ShaderResourceView(std::move(srvAllocs[i]));
        }
        CreateSceneColorBuffers(width, height);
    }

    // GBuffers
    {
        auto rtvAllocs = gBufferRtvAllocation.Split();
        auto srvAllocs = gBufferSrvAllocation.Split();
        for (UINT i = 0; i < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++i)
        {
            m_gBufferRtvs[i] = RenderTargetView(std::move(rtvAllocs[i]));
            m_gBufferSrvs[i] = ShaderResourceView(std::move(srvAllocs[i]));
        }
        CreateGBuffers(width, height);
    }

    // Create masks
    m_selectionMaskRtv = RenderTargetView(std::move(selectionMaskRtvAllocation));
    m_selectionMaskSrv = ShaderResourceView(std::move(selectionMaskSrvAllocation));
    m_horizontalDilatedMaskRtv = RenderTargetView(std::move(horizontalDilatedMaskRtvAllocation));
    m_horizontalDilatedMaskSrv = ShaderResourceView(std::move(horizontalDilatedMaskSrvAllocation));
    CreateMasks(width, height);

    // ToneMappedBuffer
    m_toneMappedBufferSrv = ImGuiShaderResourceView(std::move(toneMappedBufferSrvAllocation));
    m_toneMappedBufferRtv = RenderTargetView(std::move(toneMappedBufferRtvAllocation));
    CreateToneMappedBuffer(width, height);

    // Create Upload buffer
    m_instanceUploadBuffer = Buffer(m_pDevice, sizeof(InstanceData) * m_instanceCapacity, D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RANGE readRange = {0, 0};
    ThrowIfFailed(m_instanceUploadBuffer.Get()->Map(0, &readRange, reinterpret_cast<void**>(&m_instanceBufferBegin)));

    m_uploadAllocator.Init(pDevice);
}

// Back buffer
void FrameResource::AcquireBackBuffer(IDXGISwapChain* pSwapChain, UINT frameIndex)
{
    ComPtr<ID3D12Resource> backBuffer;
    ThrowIfFailed(pSwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&backBuffer)));
    m_backBuffer = Texture(std::move(backBuffer));
    m_backBufferRtv.Init(m_pDevice, m_backBuffer.Get(), GetRtvDesc(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 0));
}

ID3D12Resource* FrameResource::GetBackBuffer() const
{
    return m_backBuffer.Get();
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

// ToneMappedBuffer
void FrameResource::CreateToneMappedBuffer(UINT64 width, UINT height)
{
    const auto format = DXGI_FORMAT_R8G8B8A8_UNORM;
    const auto clearValue = CreateClearValue(format, 0.0f, 0.0f, 0.0f, 0.0f);

    m_toneMappedBuffer = Texture(
        m_pDevice,
        GetTexture2DDesc(width, height, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        &clearValue);

    m_toneMappedBufferRtv.Init(m_pDevice, m_toneMappedBuffer.Get(), GetRtvDesc(format, 0));
    m_toneMappedBufferSrv.Init(m_pDevice, m_toneMappedBuffer.Get(), GetSrvDesc(format, 1));
}

ID3D12Resource* FrameResource::GetToneMappedBuffer() const
{
    return m_toneMappedBuffer.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetToneMappedBufferRtvHandle() const
{
    return m_toneMappedBufferRtv.GetHandle();
}

D3D12_GPU_DESCRIPTOR_HANDLE FrameResource::GetToneMappedBufferSrvHandle() const
{
    return m_toneMappedBufferSrv.GetGpuHandle();
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
        D3D12_RANGE readRange = {0, 0};
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
UINT64 FrameResource::GetSignaledFenceValue() const
{
    return m_signaledFenceValue;
}

void FrameResource::UpdateSignaledFenceValue(UINT64 signaledFenceValue)
{
    m_signaledFenceValue = signaledFenceValue;
}
