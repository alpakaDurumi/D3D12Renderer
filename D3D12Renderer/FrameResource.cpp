#include "pch.h"
#include "FrameResource.h"

#include "D3DHelper.h"
#include "Utility.h"

FrameResource::FrameResource(
    ID3D12Device10* pDevice,
    IDXGISwapChain* pSwapChain,
    UINT frameIndex,
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

    AcquireBackBuffer(pSwapChain, frameIndex);

    CreateRTV(m_pDevice, m_renderTarget.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, m_rtvAllocation.GetDescriptorHandle());

    CreateUploadBuffer(m_pDevice, sizeof(InstanceData) * m_instanceCapacity, m_instanceUploadBuffer);
    D3D12_RANGE readRange = { 0, 0 };
    D3DHelper::ThrowIfFailed(m_instanceUploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_instanceBufferBegin)));

    // Create deffered rendering stuffs
    auto rtDesc = m_renderTarget->GetDesc();
    const UINT64 width = rtDesc.Width;
    const UINT height = rtDesc.Height;

    CreateGBuffers(width, height);
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

void FrameResource::CreateGBuffers(UINT64 width, UINT height)
{
    for (UINT i = 0; i < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++i)
    {
        auto format = GetGBufferFormat(static_cast<GBufferSlot>(i));

        CreateRenderTarget(m_pDevice, width, height, format, format, 1, m_gBuffers[i]);

        CreateRTV(m_pDevice, m_gBuffers[i].Get(), format, m_gBufferRTVAllocation.GetDescriptorHandle(i));
        CreateSRV(m_pDevice, m_gBuffers[i].Get(), format, m_gBufferSRVAllocation.GetDescriptorHandle(i));
    }
}

void FrameResource::AcquireBackBuffer(IDXGISwapChain* pSwapChain, UINT frameIndex)
{
    D3DHelper::ThrowIfFailed(pSwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_renderTarget)));
}

ID3D12Resource* FrameResource::GetRenderTarget() const
{
    return m_renderTarget.Get();
}

ID3D12Resource* FrameResource::GetGBuffer(GBufferSlot slot) const
{
    return m_gBuffers[static_cast<UINT>(slot)].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetRTVHandle() const
{
    return m_rtvAllocation.GetDescriptorHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE FrameResource::GetGBufferRTVHandle(GBufferSlot slot) const
{
    return m_gBufferRTVAllocation.GetDescriptorHandle(static_cast<UINT>(slot));
}

std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> FrameResource::GetGBufferRTVHandles() const
{
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> ret(static_cast<std::size_t>(GBufferSlot::NUM_GBUFFER_SLOTS));
    for (UINT i = 0; i < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++i)
    {
        ret[i] = m_gBufferRTVAllocation.GetDescriptorHandle(i);
    }
    return ret;
}

DescriptorAllocation& FrameResource::GetGBufferSRVAllocationRef()
{
    return m_gBufferSRVAllocation;
}

UINT64 FrameResource::GetFenceValue() const
{
    return m_fenceValue;
}

UINT FrameResource::GetCameraConstantBufferCount() const
{
    return static_cast<UINT>(m_cameraConstantBuffers.size());
}

UINT FrameResource::GetMaterialConstantBufferCount() const
{
    return static_cast<UINT>(m_materialConstantBuffers.size());
}

void FrameResource::AddCameraConstantBuffer()
{
    m_cameraConstantBuffers.push_back(std::make_unique<CameraCB>(m_pDevice));
}

void FrameResource::CreateShadowConstantBuffer()
{
    m_shadowConstantBuffer = std::make_unique<ShadowCB>(m_pDevice);
}

void FrameResource::AddLightConstantBuffer(DescriptorAllocation&& allocation)
{
    m_lightConstantBuffers.push_back(std::make_unique<LightCB>(m_pDevice, std::move(allocation)));
}

void FrameResource::AddMaterialConstantBuffer(DescriptorAllocation&& allocation)
{
    m_materialConstantBuffers.push_back(std::make_unique<MaterialCB>(m_pDevice, std::move(allocation)));
}

DescriptorAllocation& FrameResource::GetMaterialCBVAllocationRef(UINT idx)
{
    return m_materialConstantBuffers[idx]->GetAllocationRef();
}

DescriptorAllocation& FrameResource::GetLightCBVAllocationRef(UINT idx)
{
    return m_lightConstantBuffers[idx]->GetAllocationRef();
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResource::GetCameraCBVirtualAddress(UINT idx) const
{
    return m_cameraConstantBuffers[idx]->GetGPUVirtualAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS FrameResource::GetShadowCBVirtualAddress() const
{
    return m_shadowConstantBuffer->GetGPUVirtualAddress();
}

void FrameResource::UpdateCameraConstantBuffer(UINT idx, CameraConstantData* pData)
{
    m_cameraConstantBuffers[idx]->Update(pData);
}

void FrameResource::UpdateShadowConstantBuffer(ShadowConstantData* pData)
{
    m_shadowConstantBuffer->Update(pData);
}

void FrameResource::UpdateMaterialConstantBuffer(UINT idx, MaterialConstantData* pData)
{
    m_materialConstantBuffers[idx]->Update(pData);
}

void FrameResource::UpdateLightConstantBuffer(UINT idx, LightConstantData* pData)
{
    m_lightConstantBuffers[idx]->Update(pData);
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

void FrameResource::SetFenceValue(UINT64 fenceValue)
{
    m_fenceValue = fenceValue;
}

void FrameResource::ResetRenderTarget()
{
    m_renderTarget.Reset();
}

void FrameResource::ResetGBuffer(GBufferSlot slot)
{
    m_gBuffers[static_cast<UINT>(slot)].Reset();
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