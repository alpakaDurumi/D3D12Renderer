#pragma once

#include <Windows.h>

#include <optional>

#include "GpuResource.h"
#include "DescriptorAllocation.h"

class DepthStencilBuffer : public GpuResource
{
public:
    DepthStencilBuffer(
        ID3D12Device10* pDevice,
        UINT width,
        UINT height,
        bool useStencil,
        DescriptorAllocation&& dsvAllocation);

    DepthStencilBuffer(
        ID3D12Device10* pDevice,
        UINT width,
        UINT height,
        bool useStencil,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation);

    void Init(
        ID3D12Device10* pDevice,
        UINT width,
        UINT height,
        bool useStencil);

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetReadOnlyDSVHandle() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const;

private:
    DescriptorAllocation m_dsvAllocation;
    std::optional<DescriptorAllocation> m_srvAllocation;
};