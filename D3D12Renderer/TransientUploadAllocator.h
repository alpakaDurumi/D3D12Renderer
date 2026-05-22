#pragma once

#include <minwindef.h>
#include <basetsd.h>

#include <d3d12.h>

#include <vector>
#include <memory>
#include <cstddef>
#include <utility>
#include <cstring>

#include "Utility.h"
#include "UploadAllocation.h"
#include "Buffer.h"

// Linear allocator using upload heap for transient usage.
// Fence of CommandQueue ensures the safety of transient allocations,
// So no per-allocation tracking is required.
// Do not use this for cross-frame allocation.
class TransientUploadAllocator
{
public:
    // Disable copy and move
    TransientUploadAllocator(const TransientUploadAllocator&) = delete;
    TransientUploadAllocator& operator=(const TransientUploadAllocator&) = delete;
    TransientUploadAllocator(TransientUploadAllocator&&) = delete;
    TransientUploadAllocator& operator=(TransientUploadAllocator&&) = delete;

    TransientUploadAllocator(ID3D12Device10* pDevice)
        : m_pDevice(pDevice)
    {
        AllocatePage();
        m_currentPageIndex = 0;
        m_currentOffset = 0;
    }

    // Only allocate space
    UploadAllocation Allocate(std::size_t size, std::size_t alignment)
    {
        m_currentOffset = Utility::Align(m_currentOffset, alignment);

        // If current page has not enough space
        if (m_currentOffset + size > PAGE_SIZE)
        {
            // If there is no spare page
            if (m_currentPageIndex + 1 == m_pages.size())
                AllocatePage();

            ++m_currentPageIndex;
            m_currentOffset = 0;
        }

        auto& currentPage = m_pages[m_currentPageIndex];

        UploadAllocation alloc = {
            currentPage->uploadBuffer.Get(),
            m_currentOffset,
            static_cast<UINT8*>(currentPage->cpuBasePtr) + m_currentOffset,
            currentPage->gpuBasePtr + m_currentOffset
        };

        m_currentOffset += size;

        return alloc;
    }

    // Allocate and copy data from src to currentOffset, returns Allocation
    UploadAllocation Push(void* src, std::size_t size, std::size_t alignment)
    {
        auto alloc = Allocate(size, alignment);
        if (src) std::memcpy(alloc.cpuPtr, src, size);
        return alloc;
    }

    // Should be called at each frame start
    void Reset()
    {
        m_currentPageIndex = 0;
        m_currentOffset = 0;
    }

private:
    static const UINT64 PAGE_SIZE = 16 * 1024 * 1024;     // 16MB

    void AllocatePage()
    {
        auto page = std::make_unique<Page>(m_pDevice);
        m_pages.push_back(std::move(page));
    }

    struct Page
    {
        Page(ID3D12Device10* pDevice)
        {
            uploadBuffer = Buffer(pDevice, PAGE_SIZE, D3D12_HEAP_TYPE_UPLOAD);

            D3D12_RANGE readRange = { 0, 0 };
            uploadBuffer.Get()->Map(0, &readRange, &cpuBasePtr);
            gpuBasePtr = uploadBuffer.Get()->GetGPUVirtualAddress();
        }

        ~Page()
        {
            uploadBuffer.Get()->Unmap(0, nullptr);
        }

        Buffer uploadBuffer;
        void* cpuBasePtr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuBasePtr;
    };

    ID3D12Device10* m_pDevice;
    std::vector<std::unique_ptr<Page>> m_pages;
    UINT m_currentPageIndex;
    UINT64 m_currentOffset;
};