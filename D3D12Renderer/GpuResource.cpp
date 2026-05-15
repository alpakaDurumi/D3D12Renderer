#include "pch.h"
#include "GpuResource.h"

GpuResource::GpuResource(Microsoft::WRL::ComPtr<ID3D12Resource>&& existing)
    : m_resource(std::move(existing))
{
}

ID3D12Resource* GpuResource::Get() const
{
    return m_resource.Get();
}

void GpuResource::Reset()
{
    m_resource.Reset();
}