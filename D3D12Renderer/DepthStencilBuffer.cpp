#include "pch.h"
#include "DepthStencilBuffer.h"

#include "D3DHelper.h"

using namespace D3DHelper;

DepthStencilBuffer::DepthStencilBuffer(
    ID3D12Device10* pDevice,
    UINT width,
    UINT height,
    bool useStencil,
    DescriptorAllocation&& dsvAllocation)
    : m_dsvAllocation(std::move(dsvAllocation))
{
    Init(pDevice, width, height, useStencil);
}

DepthStencilBuffer::DepthStencilBuffer(
    ID3D12Device10* pDevice,
    UINT width,
    UINT height,
    bool useStencil,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation)
    : m_dsvAllocation(std::move(dsvAllocation)), m_srvAllocation(std::move(srvAllocation))
{
    Init(pDevice, width, height, useStencil);
}

void DepthStencilBuffer::Init(
    ID3D12Device10* pDevice,
    UINT width,
    UINT height,
    bool useStencil)
{
    CreateDepthStencilBuffer(pDevice, width, height, 1, m_resource, useStencil);
    CreateDSV(pDevice, Get(), GetDSVHandle(), useStencil, false);
    if (m_dsvAllocation.GetNumHandles() == 2)
        CreateDSV(pDevice, Get(), GetReadOnlyDSVHandle(), useStencil, true);
    if(m_srvAllocation.has_value())
        CreateSRV(pDevice, Get(), useStencil ? DXGI_FORMAT_R24_UNORM_X8_TYPELESS : DXGI_FORMAT_R32_FLOAT, GetSRVHandle(), 0);
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilBuffer::GetDSVHandle() const
{
    return m_dsvAllocation.GetDescriptorHandle(0);
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilBuffer::GetReadOnlyDSVHandle() const
{
    return m_dsvAllocation.GetDescriptorHandle(1);
}

D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilBuffer::GetSRVHandle() const
{
    if (!m_srvAllocation.has_value())
        return { 0 };
    return m_srvAllocation->GetDescriptorHandle();
}