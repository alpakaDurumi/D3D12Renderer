#include "UploadBuffer.h"
#include "D3DHelper.h"
#include <new>

using namespace D3DHelper;

SIZE_T Align(SIZE_T size, SIZE_T alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

// UploadBuffer

UploadBuffer::UploadBuffer(ComPtr<ID3D12Device>& device, SIZE_T pageSize)
    : m_device(device), m_pageSize(pageSize)
{
}

UploadBuffer::Allocation UploadBuffer::Allocate(SIZE_T sizeInBytes, SIZE_T alignment)
{
    if (sizeInBytes > m_pageSize)
    {
        throw std::bad_alloc();
    }

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
    m_availablePages = m_pagePool;

    for (auto page : m_availablePages)
    {
        page->Reset();
    }
}

// Page

UploadBuffer::Page::Page(ComPtr<ID3D12Device>& device, SIZE_T sizeInBytes)
    : m_pageSize(sizeInBytes),
    m_offset(0),
    m_CPUPtr(nullptr),
    m_GPUPtr(D3D12_GPU_VIRTUAL_ADDRESS(0))
{
    CreateUploadHeap(device, m_pageSize, m_resource);

    m_GPUPtr = m_resource->GetGPUVirtualAddress();
    m_resource->Map(0, nullptr, &m_CPUPtr);
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
    if (!HasSpace(sizeInBytes, alignment))
    {
        throw std::bad_alloc();
    }

    SIZE_T alignedSize = Align(sizeInBytes, alignment);
    m_offset = Align(m_offset, alignment);

    Allocation allocation;
    allocation.CPU = static_cast<uint8_t*>(m_CPUPtr) + m_offset;
    allocation.GPU = m_GPUPtr + m_offset;

    m_offset += alignedSize;

    return allocation;
}

void UploadBuffer::Page::Reset()
{
    m_offset = 0;
}