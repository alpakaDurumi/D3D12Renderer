#include "pch.h"
#include "Texture.h"

#include <cassert>

Texture::Texture(DescriptorAllocation&& srvAllocation)
    : m_srvAllocation(std::move(srvAllocation))
{
    assert(!m_srvAllocation.IsNull());
}

ID3D12Resource* Texture::Get() const
{
    return m_resource.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetSRVHandle() const
{
    return m_srvAllocation.GetDescriptorHandle();
}

void Texture::Reset()
{
    m_resource.Reset();
}