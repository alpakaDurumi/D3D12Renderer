#pragma once

#include "GpuResource.h"
#include "DescriptorAllocation.h"

class Texture : public GpuResource
{
public:
    Texture(DescriptorAllocation&& srvAllocation);

    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const;

protected:
    DescriptorAllocation m_srvAllocation;
};