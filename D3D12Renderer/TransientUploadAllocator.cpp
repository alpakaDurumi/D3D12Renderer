#include "pch.h"

#include "TransientUploadAllocator.h"

#include "UploadAllocation.h"
#include "Utility.h"

TransientUploadAllocator::TransientUploadAllocator(ID3D12Device10* pDevice)
    : m_pDevice(pDevice)
{
    AllocatePage();
    m_currentPageIndex = 0;
    m_currentOffset = 0;
}

TransientUploadAllocator::~TransientUploadAllocator() = default;

// Only allocate space
UploadAllocation TransientUploadAllocator::Allocate(std::size_t size, std::size_t alignment)
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
        currentPage->gpuBasePtr + m_currentOffset};

    m_currentOffset += size;

    return alloc;
}

// Allocate and copy data from src to currentOffset, returns Allocation
UploadAllocation TransientUploadAllocator::Push(void* src, std::size_t size, std::size_t alignment)
{
    auto alloc = Allocate(size, alignment);
    if (src)
        std::memcpy(alloc.cpuPtr, src, size);
    return alloc;
}

// Should be called at each frame start
void TransientUploadAllocator::Reset()
{
    m_currentPageIndex = 0;
    m_currentOffset = 0;
}

void TransientUploadAllocator::AllocatePage()
{
    auto page = std::make_unique<Page>(m_pDevice);
    m_pages.push_back(std::move(page));
}

TransientUploadAllocator::Page::Page(ID3D12Device10* pDevice)
{
    uploadBuffer = Buffer(pDevice, PAGE_SIZE, D3D12_HEAP_TYPE_UPLOAD);

    D3D12_RANGE readRange = {0, 0};
    uploadBuffer.Get()->Map(0, &readRange, &cpuBasePtr);
    gpuBasePtr = uploadBuffer.Get()->GetGPUVirtualAddress();
}

TransientUploadAllocator::Page::~Page()
{
    uploadBuffer.Get()->Unmap(0, nullptr);
}
