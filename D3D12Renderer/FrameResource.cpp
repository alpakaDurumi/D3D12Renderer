#include "pch.h"
#include "FrameResource.h"

#include "D3DHelper.h"
#include "Utility.h"
#include "SharedConfig.h"

FrameResource::FrameResource(
    ID3D12Device10* pDevice,
    IDXGISwapChain* pSwapChain,
    UINT frameIndex,
    ResourceLayoutTracker& layoutTracker,
    DescriptorAllocation&& rtvAllocation,
    DescriptorAllocation&& gBufferRTVAllocation,
    DescriptorAllocation&& gBufferSRVAllocation)
    : m_pDevice(pDevice),
    m_rtvAllocation(std::move(rtvAllocation)),
    m_gBufferRTVAllocation(std::move(gBufferRTVAllocation)),
    m_gBufferSRVAllocation(std::move(gBufferSRVAllocation))
{
    assert(!m_rtvAllocation.IsNull() &&
        m_gBufferRTVAllocation.GetNumHandles() == static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS) &&
        m_gBufferSRVAllocation.GetNumHandles() == static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS));

    D3DHelper::ThrowIfFailed(pSwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_renderTarget)));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    m_pDevice->CreateRenderTargetView(m_renderTarget.Get(), &rtvDesc, m_rtvAllocation.GetDescriptorHandle());

    CreateUploadBuffer(m_pDevice, sizeof(InstanceData) * m_instanceCapacity, m_instanceUploadBuffer);
    D3D12_RANGE readRange = { 0, 0 };
    D3DHelper::ThrowIfFailed(m_instanceUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_instanceBufferBegin)));

    // Create deffered rendering stuffs
    auto rtDesc = m_renderTarget->GetDesc();
    const UINT64 width = rtDesc.Width;
    const UINT64 height = rtDesc.Height;

    for (UINT i = 0; i < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++i)
    {
        CreateRenderTarget(m_pDevice, width, height, (i == 0 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R16G16B16A16_FLOAT), 1, m_gBuffers[i]);
        layoutTracker.RegisterResource(m_gBuffers[i].Get(), D3D12_BARRIER_LAYOUT_RENDER_TARGET, 1, 1, (i == 0 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R16G16B16A16_FLOAT));

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = (i == 0 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R16G16B16A16_FLOAT);
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_pDevice->CreateRenderTargetView(m_gBuffers[i].Get(), &rtvDesc, m_gBufferRTVAllocation.GetDescriptorHandle(i));

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = (i == 0 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R16G16B16A16_FLOAT);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        pDevice->CreateShaderResourceView(m_gBuffers[i].Get(), &srvDesc, m_gBufferSRVAllocation.GetDescriptorHandle(i));
    }
}

FrameResource::~FrameResource()
{
    m_instanceUploadBuffer->Unmap(0, nullptr);
}

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

        CreateUploadBuffer(m_pDevice, sizeof(InstanceData) * m_instanceCapacity, m_instanceUploadBuffer);
        D3D12_RANGE readRange = { 0, 0 };
        D3DHelper::ThrowIfFailed(m_instanceUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_instanceBufferBegin)));
    }
}