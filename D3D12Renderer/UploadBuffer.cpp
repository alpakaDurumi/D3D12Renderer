#include "UploadBuffer.h"
#include "D3DHelper.h"
#include <new>

using namespace D3DHelper;

SIZE_T Align(SIZE_T size, SIZE_T alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

// UploadBuffer

UploadBuffer::UploadBuffer(ComPtr<ID3D12Device10>& device, SIZE_T pageSize)
    : m_device(device), m_pageSize(pageSize)
{
}

UploadBuffer::Allocation UploadBuffer::Allocate(SIZE_T sizeInBytes, SIZE_T alignment)
{
    if (sizeInBytes > m_pageSize)
    {
        throw std::bad_alloc();
    }

    // 첫 할당이거나 현재 Page의 공간이 부족한 경우
    if (!m_currentPage || !m_currentPage->HasSpace(sizeInBytes, alignment))
    {
        m_currentPage = RequestPage();
    }

    return m_currentPage->Allocate(sizeInBytes, alignment);
}

std::shared_ptr<UploadBuffer::Page> UploadBuffer::RequestPage()
{
    std::shared_ptr<Page> page;

    if (!m_availablePages.empty())
    {
        page = m_availablePages.front();
        m_availablePages.pop_front();
    }
    else
    {
        page = std::make_shared<Page>(m_device, m_pageSize);
        m_pagePool.push_back(page);
    }

    return page;
}

void UploadBuffer::Reset()
{
    m_currentPage = nullptr;

    // Set all pages to available
    m_availablePages = m_pagePool;

    // Set offset of all pages to 0
    for (auto page : m_availablePages)
    {
        page->Reset();
    }
}

// Page

// Page가 살아있는 동안 한 Page 전체 영역에 대해 Mapping이 유지된다. 소멸자가 호출되면 Unmap을 통해 Mapping이 해제된다.
UploadBuffer::Page::Page(ComPtr<ID3D12Device10>& device, SIZE_T sizeInBytes)
    : m_pageSize(sizeInBytes),
    m_offset(0),
    m_CPUBasePtr(nullptr),
    m_GPUBasePtr(D3D12_GPU_VIRTUAL_ADDRESS(0))
{
    CreateUploadHeap(device, m_pageSize, m_resource);
    
    D3D12_RANGE readRange = { 0, 0 };
    m_resource->Map(0, &readRange, &m_CPUBasePtr);
    m_GPUBasePtr = m_resource->GetGPUVirtualAddress();
}

UploadBuffer::Page::~Page()
{
    m_resource->Unmap(0, nullptr);
}

bool UploadBuffer::Page::HasSpace(SIZE_T sizeInBytes, SIZE_T alignment) const
{
    SIZE_T alignedSize = Align(sizeInBytes, alignment);
    SIZE_T alignedOffset = Align(m_offset, alignment);

    return alignedOffset + alignedSize <= m_pageSize;
}

UploadBuffer::Allocation UploadBuffer::Page::Allocate(SIZE_T sizeInBytes, SIZE_T alignment)
{
    SIZE_T alignedSize = Align(sizeInBytes, alignment);
    m_offset = Align(m_offset, alignment);
    
    Allocation allocation;
    allocation.m_CPUPtr = static_cast<void*>(static_cast<UINT8*>(m_CPUBasePtr) + m_offset);
    allocation.m_GPUPtr = m_GPUBasePtr + m_offset;

    m_offset += alignedSize;

    return allocation;
}

void UploadBuffer::Page::Reset()
{
    m_offset = 0;
}