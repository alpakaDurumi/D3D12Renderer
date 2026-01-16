#include "pch.h"
#include "DescriptorAllocator.h"

#include "CommandQueue.h"

DescriptorAllocator::DescriptorAllocator(const ComPtr<ID3D12Device10>& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT32 numDescriptorsPerHeap)
    : m_device(device), m_heapType(type), m_numDescriptorsPerHeap(numDescriptorsPerHeap)
{
}

// Allocate contiguous block of descriptors from heap
DescriptorAllocation DescriptorAllocator::Allocate(UINT32 numDescriptors)
{
    std::lock_guard<std::mutex> lock(m_allocationMutex);

    // Release allocations that have finished execution before allocation.
    ReleaseStaleDescriptors(m_pCommandQueue->GetCompletedFenceValue());

    std::optional<DescriptorAllocation> allocation;

    auto it = m_availableHeaps.begin();
    while (it != m_availableHeaps.end())
    {
        auto& allocatorPage = m_heapPool[*it];

        allocation = allocatorPage->Allocate(numDescriptors);

        // A valid allocation has been found
        if (allocation.has_value())
        {
            // In DescriptorAllocatorPage::Allocate, m_numFreeHandles is already checked,
            // but here it's checked again with GetNumFreeHandles. This could be redundant.
            
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
    if (!allocation.has_value())
    {
        // Increase page size for demand
        m_numDescriptorsPerHeap = std::max(m_numDescriptorsPerHeap, numDescriptors);
        auto newPage = CreateAllocatorPage();

        allocation = newPage->Allocate(numDescriptors);
    }

    // std::optional<T>::value returns l-value reference, so std::move should be used for satisfy
    // move constructor/assignment of DescriptorAllocation
    return std::move(allocation.value());
}

// Create a new heap with a specific number of descriptors
DescriptorAllocatorPage* DescriptorAllocator::CreateAllocatorPage()
{
    m_heapPool.emplace_back(std::make_unique<DescriptorAllocatorPage>(m_device.Get(), m_heapType, m_numDescriptorsPerHeap));
    m_availableHeaps.insert(m_heapPool.size() - 1);     // Index of the page added
    return m_heapPool.back().get();
}

void DescriptorAllocator::ReleaseStaleDescriptors(UINT64 completedFenceValue)
{
    for (SIZE_T i = 0; i < m_heapPool.size(); ++i)
    {
        auto& page = m_heapPool[i];

        page->ReleaseStaleDescriptors(completedFenceValue);

        if (page->GetNumFreeHandles() > 0)
        {
            m_availableHeaps.insert(i);
        }
    }
}