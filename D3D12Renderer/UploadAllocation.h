#pragma once

#include <basetsd.h>

#include <d3d12.h>

struct UploadAllocation
{
    ID3D12Resource* pResource;
    UINT64 offset;                      // Offset in resource
    void* cpuPtr;                       // Ptr that offset from resource's mapped ptr
    D3D12_GPU_VIRTUAL_ADDRESS gpuPtr;
};