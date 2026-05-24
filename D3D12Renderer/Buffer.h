#pragma once

#include <basetsd.h>

#include <d3d12.h>

#include "GpuResource.h"

class Buffer : public GpuResource
{
public:
    using GpuResource::GpuResource; // Inheriting Constructor

    Buffer(ID3D12Device10* pDevice, UINT64 width, D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT);
};
