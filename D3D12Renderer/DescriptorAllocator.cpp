#define NOMINMAX

#include "DescriptorAllocator.h"
#include "DescriptorAllocatorPage.h"
#include "DescriptorAllocation.h"

DescriptorAllocator::DescriptorAllocator(ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT32 numDescriptorsPerHeap)
    : m_device(device), m_heapType(type), m_numDescriptorsPerHeap(numDescriptorsPerHeap)
{
}

// Allocate contiguous block of descriptors from heap
DescriptorAllocation DescriptorAllocator::Allocate(UINT32 numDescriptors)
{
    std::lock_guard<std::mutex> lock(m_allocationMutex);

    DescriptorAllocation allocation;

    auto it = m_availableHeaps.begin();
    while (it != m_availableHeaps.end())
    {
        auto allocatorPage = m_heapPool[*it];

        allocation = allocatorPage->Allocate(numDescriptors);

        // A valid allocation has been found
        if (!allocation.IsNull())
        {
            // DescriptorAllocatorPage::Allocate 내에서 m_numFreeHandles를 감소시킴에도 불구하고,
            // 다시 GetNumFreeHandles를 호출하여 개수를 확인하고 있다. DescriptorAllocatorPage::Allocate에서
            // 남은 개수가 0인지 아닌지도 반환하도록 하면 연산 횟수를 줄일 수 있을 것 같다.
            
            // Check if page has free handles only when allocation succeeded
            if (allocatorPage->GetNumFreeHandles() == 0)
            {
                m_availableHeaps.erase(it);
            }
            break;
        }
        else
        {
            ++it;
        }
    }

    // No available heap could satisfy the requested number of descriptors
    if (allocation.IsNull())
    {
        // Increase page size for demand
        m_numDescriptorsPerHeap = std::max(m_numDescriptorsPerHeap, numDescriptors);
        auto newPage = CreateAllocatorPage();

        allocation = newPage->Allocate(numDescriptors);
    }

    return allocation;
}

// This function not use mutex since it assumes that mutex already locked on caller's side
// If this function called outside of DescriptorAllocator::Allocate, explicit mutex should be locked
std::shared_ptr<DescriptorAllocatorPage> DescriptorAllocator::CreateAllocatorPage()
{
    auto newPage = std::make_shared<DescriptorAllocatorPage>(m_device, m_heapType, m_numDescriptorsPerHeap);
    m_heapPool.push_back(newPage);
    m_availableHeaps.insert(m_heapPool.size() - 1);     // Index of the page added

    return newPage;
}

void DescriptorAllocator::ReleaseStaleDescriptors(UINT64 completedFenceValue)
{
    std::lock_guard<std::mutex> lock(m_allocationMutex);

    for (SIZE_T i = 0; i < m_heapPool.size(); ++i)
    {
        auto page = m_heapPool[i];

        page->ReleaseStaleDescriptors(completedFenceValue);

        if (page->GetNumFreeHandles() > 0)
        {
            m_availableHeaps.insert(i);
        }
    }
}