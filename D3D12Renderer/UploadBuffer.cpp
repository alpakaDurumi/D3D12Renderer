#include "pch.h"
#include "UploadBuffer.h"

#include "D3DHelper.h"
#include "CommandQueue.h"

using namespace D3DHelper;

SIZE_T Align(SIZE_T size, SIZE_T alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

UploadBuffer::Allocation::Allocation(UploadBuffer::Allocation&& other) noexcept
    : CPUPtr(other.CPUPtr), GPUPtr(other.GPUPtr), pResource(other.pResource), Offset(other.Offset)
{
    other.CPUPtr = nullptr;
    other.GPUPtr = 0;
    other.pResource = nullptr;
    other.Offset = 0;
}

UploadBuffer::Allocation& UploadBuffer::Allocation::operator=(UploadBuffer::Allocation&& other) noexcept
{
    if (this != &other)
    {
        CPUPtr = other.CPUPtr;
        GPUPtr = other.GPUPtr;
        pResource = other.pResource;
        Offset = other.Offset;

        other.CPUPtr = nullptr;
        other.GPUPtr = 0;
        other.pResource = nullptr;
        other.Offset = 0;
    }

    return *this;
}

UploadBuffer::UploadBuffer(const ComPtr<ID3D12Device10>& device, SIZE_T pageSize)
    : m_device(device), m_pCommandQueue(nullptr), m_pageSize(pageSize), m_currentPage(nullptr), m_currentOffset(0)
{
}

UploadBuffer::Allocation UploadBuffer::Allocate(SIZE_T sizeInBytes, SIZE_T alignment)
{
    const SIZE_T alignedSize = Align(sizeInBytes, alignment);

    if (alignedSize > m_pageSize)
    {
        // When requested size is larger than current page size
        // Create page with requested size and restore original size
        auto originalSize = m_pageSize;
        m_pageSize = alignedSize;
        m_currentPage = RequestPage();
        m_pageSize = originalSize;

        m_currentOffset = 0;
    }
    else
    {
        m_currentOffset = Align(m_currentOffset, alignment);
    }

    // First allocation or current page has not enough space
    if (!m_currentPage || (m_currentOffset + alignedSize > m_pageSize))
    {
        if (m_currentPage)
        {
            m_retiredPages.push_back(m_currentPage);
        }
        m_currentPage = RequestPage();
        m_currentOffset = 0;
    }

    Allocation allocation(
        static_cast<void*>(static_cast<UINT8*>(m_currentPage->m_CPUBasePtr) + m_currentOffset),
        m_currentPage->m_GPUBasePtr + m_currentOffset,
        m_currentPage->m_resource.Get(),
        m_currentOffset);

    m_currentOffset += alignedSize;

    return allocation;
}

UploadBuffer::Page* UploadBuffer::RequestPage()
{
    while (!m_pendingPages.empty() && m_pCommandQueue->IsFenceComplete(m_pendingPages.front().first))
    {
        m_availablePages.push(m_pendingPages.front().second);
        m_pendingPages.pop();
    }

    Page* pPage;

    if (!m_availablePages.empty())
    {
        pPage = m_availablePages.front();
        m_availablePages.pop();
    }
    else
    {
        m_pagePool.emplace_back(std::make_unique<Page>(m_device.Get(), m_pageSize));
        pPage = m_pagePool.back().get();
    }

    return pPage;
}

// Move retired pages to pending queue to wait for specific fenceValue
void UploadBuffer::QueueRetiredPages(UINT64 fenceValue)
{
    for (auto* page : m_retiredPages)
    {
        m_pendingPages.push({ fenceValue, page });
    }
    m_retiredPages.clear();
}

// Keep memory mapping for the page's lifetime
UploadBuffer::Page::Page(ID3D12Device10* pDevice, SIZE_T sizeInBytes)
    : m_pageSize(sizeInBytes),
    m_CPUBasePtr(nullptr),
    m_GPUBasePtr(D3D12_GPU_VIRTUAL_ADDRESS(0))
{
    CreateUploadHeap(pDevice, m_pageSize, m_resource);
    
    D3D12_RANGE readRange = { 0, 0 };
    m_resource->Map(0, &readRange, &m_CPUBasePtr);
    m_GPUBasePtr = m_resource->GetGPUVirtualAddress();
}

UploadBuffer::Page::~Page()
{
    m_resource->Unmap(0, nullptr);
}