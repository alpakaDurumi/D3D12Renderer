#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include <basetsd.h>
#include <d3d12.h>
#include <minwindef.h>

#include "Buffer.h"

struct UploadAllocation;

// Linear allocator using upload heap for transient usage.
// Fence of CommandQueue ensures the safety of transient allocations,
// So no per-allocation tracking is required.
// Do not use this for cross-frame allocation.
class TransientUploadAllocator
{
public:
    TransientUploadAllocator(const TransientUploadAllocator&) = delete;
    TransientUploadAllocator& operator=(const TransientUploadAllocator&) = delete;
    TransientUploadAllocator(TransientUploadAllocator&&) = delete;
    TransientUploadAllocator& operator=(TransientUploadAllocator&&) = delete;

    TransientUploadAllocator() = default;
    ~TransientUploadAllocator();

    void Init(ID3D12Device10* pDevice);

    UploadAllocation Allocate(std::size_t size, std::size_t alignment);
    UploadAllocation Push(void* src, std::size_t size, std::size_t alignment);

    void Reset();

private:
    static const UINT64 PAGE_SIZE = 16 * 1024 * 1024; // 16MB

    void AllocatePage();

    struct Page
    {
        Page(ID3D12Device10* pDevice);
        ~Page();

        Buffer uploadBuffer;
        void* cpuBasePtr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuBasePtr;
    };

    ID3D12Device10* m_pDevice;
    std::vector<std::unique_ptr<Page>> m_pages;
    UINT m_currentPageIndex;
    UINT64 m_currentOffset;
};
