#include "pch.h"

#include "DescriptorAllocator.h"

#include "CommandQueue.h"
#include "DescriptorAllocation.h"
#include "DescriptorAllocatorPage.h"

using Microsoft::WRL::ComPtr;

DescriptorAllocator::DescriptorAllocator() = default;
DescriptorAllocator::~DescriptorAllocator() = default;

void DescriptorAllocator::Init(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
    m_pDevice = pDevice;
    m_heapType = type;
}

void DescriptorAllocator::SetCommandQueue(const CommandQueue* pCommandQueue)
{
    m_pCommandQueue = pCommandQueue;
}

// Allocate contiguous block of descriptors from heap
DescriptorAllocation DescriptorAllocator::Allocate(UINT32 numDescriptors)
{
    assert(numDescriptors <= NumDescriptorsPerHeap);

    std::lock_guard<std::mutex> lock(m_allocationMutex);

    // Release allocations that have finished execution before allocation.
    ReleaseStaleDescriptors(m_pCommandQueue->GetCompletedFenceValue());

    std::optional<DescriptorAllocation> allocation;

    for (auto it = m_availableHeaps.begin(); it != m_availableHeaps.end(); ++it)
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
    }

    // No available heap could satisfy the requested number of descriptors
    if (!allocation.has_value())
    {
        auto* pNewPage = CreateAllocatorPage();
        allocation = pNewPage->Allocate(numDescriptors);
    }

    // std::optional<T>::value returns l-value reference, so std::move should be used for satisfy
    // move constructor/assignment of DescriptorAllocation
    return std::move(allocation.value());
}

// Create a new heap with a specific number of descriptors
DescriptorAllocatorPage* DescriptorAllocator::CreateAllocatorPage()
{
    m_heapPool.emplace_back(std::make_unique<DescriptorAllocatorPage>(m_pDevice, m_heapType, NumDescriptorsPerHeap));
    m_availableHeaps.insert(m_heapPool.size() - 1); // Index of the page added
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
