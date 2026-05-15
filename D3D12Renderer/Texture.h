#pragma once

#include <wrl/client.h>

#include <d3d12.h>

#include "GpuResource.h"

class Texture : public GpuResource
{
public:
    Texture(
        ID3D12Device10* pDevice,
        const D3D12_RESOURCE_DESC1& desc,
        D3D12_BARRIER_LAYOUT initialLayout,
        const D3D12_CLEAR_VALUE* pClearValue = nullptr,
        D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT);

    Texture(Microsoft::WRL::ComPtr<ID3D12Resource>&& other);
};