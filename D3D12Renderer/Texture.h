#pragma once

#include <wrl/client.h>

#include <d3d12.h>

#include "DescriptorAllocation.h"

using Microsoft::WRL::ComPtr;

class Texture
{
public:
    Texture(DescriptorAllocation&& srvAllocation);

    ID3D12Resource* Get() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const;
    void Reset();

protected:
    ComPtr<ID3D12Resource> m_resource;
    DescriptorAllocation m_srvAllocation;
};