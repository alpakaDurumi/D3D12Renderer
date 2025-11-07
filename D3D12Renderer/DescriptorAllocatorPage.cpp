#include "DescriptorAllocatorPage.h"
#include "DescriptorAllocation.h"
#include "D3DHelper.h"

using namespace D3DHelper;

DescriptorAllocatorPage::DescriptorAllocatorPage(ID3D12Device10* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT32 numDescriptors)
    : m_heapType(type), m_numDescriptorsInHeap(numDescriptors)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = m_heapType;
    heapDesc.NumDescriptors = m_numDescriptorsInHeap;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;   // CPU visible descriptor heap does not need to set SHADER_VISIBLE flag
    ThrowIfFailed(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));

    m_baseDescriptor = m_descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_descriptorHandleIncrementSize = pDevice->GetDescriptorHandleIncrementSize(m_heapType);
    m_numFreeHandles = m_numDescriptorsInHeap;

    // Initialize the free lists
    AddNewBlock(0, m_numFreeHandles);
}

void DescriptorAllocatorPage::AddNewBlock(UINT32 offset, UINT32 numDescriptors)
{
    auto offsetIt = m_freeListByOffset.insert({ offset, numDescriptors });
    auto sizeIt = m_freeListBySize.insert({ numDescriptors, offsetIt.first });
    offsetIt.first->second.FreeListBySizeIt = sizeIt;
}

bool DescriptorAllocatorPage::HasSpace(UINT32 numDescriptors) const
{
    return m_freeListBySize.lower_bound(numDescriptors) != m_freeListBySize.end();
}

std::optional<DescriptorAllocation> DescriptorAllocatorPage::Allocate(UINT32 numDescriptors)
{
    std::lock_guard<std::mutex> lock(m_allocationMutex);

    // Return std::nullopt if allocation failed
    if (numDescriptors > m_numFreeHandles)
    {
        return std::nullopt;
    }

    auto smallestBlockIt = m_freeListBySize.lower_bound(numDescriptors);
    if (smallestBlockIt == m_freeListBySize.end())
    {
        return std::nullopt;
    }

    auto [blockSize, offsetIt] = *smallestBlockIt;
    auto offset = offsetIt->first;

    // Remove the existing free block
    m_freeListBySize.erase(smallestBlockIt);
    m_freeListByOffset.erase(offsetIt);

    // Add remaining part as a new block
    auto newOffset = offset + numDescriptors;
    auto newSize = blockSize - numDescriptors;
    if (newSize > 0)
    {
        AddNewBlock(newOffset, newSize);
    }

    m_numFreeHandles -= numDescriptors;

    return DescriptorAllocation(m_baseDescriptor, offset, numDescriptors, m_descriptorHandleIncrementSize, this);
}

// Actual free is deferred till GPU execution using that descriptor finished
// parameter is r-value reference type
void DescriptorAllocatorPage::Free(DescriptorAllocation&& descriptor)
{
    std::lock_guard<std::mutex> lock(m_allocationMutex);

    m_staleDescriptors.emplace(descriptor.GetOffset(), descriptor.GetNumHandles(), descriptor.GetFenceValue());
}

// Entry with fenceValue that less than given (completed) fenceValue
// is popped and be the target of FreeBlock
void DescriptorAllocatorPage::ReleaseStaleDescriptors(UINT64 completedFenceValue)
{
    std::lock_guard<std::mutex> lock(m_allocationMutex);

    while (!m_staleDescriptors.empty() && m_staleDescriptors.front().FenceValue <= completedFenceValue)
    {
        auto& staleDescriptor = m_staleDescriptors.front();

        // The offset of the descriptor in the heap.
        auto offset = staleDescriptor.Offset;
        // The number of descriptors that were allocated.
        auto numDescriptors = staleDescriptor.Size;

        FreeBlock(offset, numDescriptors);

        m_staleDescriptors.pop();
    }
}

void DescriptorAllocatorPage::FreeBlock(UINT32 offset, UINT32 numDescriptors)
{
    auto nextBlockIt = m_freeListByOffset.upper_bound(offset);

    auto prevBlockIt = nextBlockIt;
    if (prevBlockIt != m_freeListByOffset.begin())
    {
        --prevBlockIt;
    }
    else
    {
        // just set it to the end of the list to indicate that no
        // block comes before the one being freed
        prevBlockIt = m_freeListByOffset.end();
    }

    m_numFreeHandles += numDescriptors;

    // Coalesce with prev block
    if (prevBlockIt != m_freeListByOffset.end() &&
        offset == prevBlockIt->first + prevBlockIt->second.Size)
    {
        // The previous block is exactly behind the block that is to be freed
        //
        // PrevBlock.Offset           Offset
        // |                          |
        // |<-----PrevBlock.Size----->|<------Size-------->|
        //

        offset = prevBlockIt->first;
        // Increase the block size by the size of merging with the previous block
        numDescriptors += prevBlockIt->second.Size;

        // Remove the previous block from the free list
        // Erase iterator that pointed by prevBlockIt
        m_freeListBySize.erase(prevBlockIt->second.FreeListBySizeIt);
        // Erase prevBlockIt itself
        m_freeListByOffset.erase(prevBlockIt);
    }

    // Coalesce with next block
    if (nextBlockIt != m_freeListByOffset.end() &&
        offset + numDescriptors == nextBlockIt->first)
    {
        // The next block is exactly in front of the block that is to be freed
        //
        // Offset               NextBlock.Offset 
        // |                    |
        // |<------Size-------->|<-----NextBlock.Size----->|

        // Increase the block size by the size of merging with the next block
        numDescriptors += nextBlockIt->second.Size;

        // Remove the next block from the free list
        m_freeListBySize.erase(nextBlockIt->second.FreeListBySizeIt);
        m_freeListByOffset.erase(nextBlockIt);
    }

    AddNewBlock(offset, numDescriptors);
}