#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include <vector>
#include <memory>
#include <cstddef>

#include "D3DHelper.h"
#include "Utility.h"
#include "UploadAllocation.h"

using Microsoft::WRL::ComPtr;

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
            currentPage->Resource.Get(),
            m_currentOffset,
            static_cast<UINT8*>(currentPage->CPUBasePtr) + m_currentOffset,
            currentPage->GPUBasePtr + m_currentOffset
        };

        m_currentOffset += size;

        return alloc;
    }

    // Allocate and copy data from src to currentOffset, returns Allocation
    UploadAllocation Push(void* src, std::size_t size, std::size_t alignment)
    {
        auto alloc = Allocate(size, alignment);
        if (src) memcpy(alloc.CPUPtr, src, size);
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
            D3DHelper::CreateUploadBuffer(pDevice, PAGE_SIZE, Resource);

            D3D12_RANGE readRange = { 0, 0 };
            Resource->Map(0, &readRange, &CPUBasePtr);
            GPUBasePtr = Resource->GetGPUVirtualAddress();
        }

        ~Page()
        {
            Resource->Unmap(0, nullptr);
        }

        ComPtr<ID3D12Resource> Resource;
        void* CPUBasePtr;
        D3D12_GPU_VIRTUAL_ADDRESS GPUBasePtr;
    };

    ID3D12Device10* m_pDevice;
    std::vector<std::unique_ptr<Page>> m_pages;
    UINT m_currentPageIndex;
    UINT64 m_currentOffset;
};