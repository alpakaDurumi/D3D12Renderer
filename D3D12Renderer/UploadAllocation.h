#pragma once

#include <Windows.h>

#include <d3d12.h>

struct UploadAllocation
{
    ID3D12Resource* pResource;
    UINT64 Offset;                      // Offset in resource
    void* CPUPtr;                       // Ptr that offset from resource's mapped ptr
    D3D12_GPU_VIRTUAL_ADDRESS GPUPtr;
};