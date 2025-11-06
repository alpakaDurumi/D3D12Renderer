#include "UploadBuffer.h"

#include <new>

#include "D3DHelper.h"
#include "CommandQueue.h"

using namespace D3DHelper;

SIZE_T Align(SIZE_T size, SIZE_T alignment)
{
    return (size + (alignment - 1)) & ~(alignment - 1);
}

// UploadBuffer

UploadBuffer::UploadBuffer(ComPtr<ID3D12Device10>& device, const CommandQueue& commandQueue, SIZE_T pageSize)
    : m_device(device), m_commandQueue(commandQueue), m_pageSize(pageSize), m_currentPage(nullptr), m_currentOffset(0)
{
}

UploadBuffer::Allocation UploadBuffer::Allocate(SIZE_T sizeInBytes, SIZE_T alignment)
{
    const SIZE_T alignedSize = Align(sizeInBytes, alignment);

    if (alignedSize > m_pageSize)
    {
        // 요구된 크기가 현재 설정된 페이지 크기보다는 크지만 최대 페이지 크기를 넘지 않는 경우
        // 요청에 맞도록 페이지 크기를 조절하여 생성한 후 다시 원상 복구
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

    // 첫 할당이거나 현재 Page의 공간이 부족한 경우
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
    while (!m_pendingPages.empty() && m_commandQueue.IsFenceComplete(m_pendingPages.front().second))
    {
        m_availablePages.push(m_pendingPages.front().first);
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

// 이번 command list 작성 동안 retire한 Page들이 특정 fenceValue가 만족되면 다시 사용될 수 있다고 큐에 넣어두는 함수.
void UploadBuffer::QueueRetiredPages(UINT64 fenceValue)
{
    for (auto* page : m_retiredPages)
    {
        m_pendingPages.push({ page, fenceValue });
    }
    m_retiredPages.clear();
}

// Page

// Page가 살아있는 동안 한 Page 전체 영역에 대해 Mapping이 유지된다. 소멸자가 호출되면 Unmap을 통해 Mapping이 해제된다.
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