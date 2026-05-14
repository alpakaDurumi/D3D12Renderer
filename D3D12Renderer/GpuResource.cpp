#include "pch.h"
#include "GpuResource.h"

ID3D12Resource* GpuResource::Get() const
{
    return m_resource.Get();
}

void GpuResource::Reset()
{
    m_resource.Reset();
}