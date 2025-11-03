#pragma once

#include <wrl/client.h>
#include <d3d12.h>

#include <memory>
#include <mutex>
#include <map>
#include <queue>
#include <optional>

class DescriptorAllocation;

using Microsoft::WRL::ComPtr;

// Wrapper for ID3D12DescriptorHeap and provides free list management
class DescriptorAllocatorPage : public std::enable_shared_from_this<DescriptorAllocatorPage>
{
public:
    DescriptorAllocatorPage(ComPtr<ID3D12Device10>& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT32 numDescriptors);

    D3D12_DESCRIPTOR_HEAP_TYPE GetHeapType() const { return m_heapType; }
    UINT32 GetNumFreeHandles() const { return m_numFreeHandles; }

    bool HasSpace(UINT32 numDescriptors) const;

    std::optional<DescriptorAllocation> Allocate(UINT32 numDescriptors);

    void Free(DescriptorAllocation&& descriptorHandle);

    void ReleaseStaleDescriptors(UINT64 completedFenceValue);

protected:
    // Adds a new block to the free list.
    void AddNewBlock(UINT32 offset, UINT32 numDescriptors);

    // Free a block of descriptors
    // This will also merge free blocks in the free list to form larger blocks
    // that can be reused
    void FreeBlock(UINT32 offset, UINT32 numDescriptors);

private:
    // The offset (in descriptors) within the descriptor heap.
    using OffsetType = UINT32;
    // The number of descriptors that are available.
    using SizeType = UINT32;

    struct FreeBlockInfo;
    // A map that lists the free blocks by the offset within the descriptor heap.
    using FreeListByOffset = std::map<OffsetType, FreeBlockInfo>;

    // A map that lists the free blocks by size.
    // Needs to be a multimap since multiple blocks can have the same size.
    using FreeListBySize = std::multimap<SizeType, FreeListByOffset::iterator>;

    struct FreeBlockInfo
    {
        FreeBlockInfo(SizeType size)
            : Size(size)
        {
        }

        SizeType Size;
        FreeListBySize::iterator FreeListBySizeIt;
    };


    struct StaleDescriptorInfo
    {
        StaleDescriptorInfo(OffsetType offset, SizeType size, UINT64 fenceValue)
            : Offset(offset), Size(size), FenceValue(fenceValue)
        {
        }

        // The offset within the descriptor heap.
        OffsetType Offset;
        // The number of descriptors
        SizeType Size;
        // The fence value that GPU execution using this descriptor ends
        UINT64 FenceValue;
    };

    using StaleDescriptorQueue = std::queue<StaleDescriptorInfo>;

    // bidirectional map
    FreeListByOffset m_freeListByOffset;
    FreeListBySize m_freeListBySize;

    StaleDescriptorQueue m_staleDescriptors;

    D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
    UINT32 m_numDescriptorsInHeap;
    ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_baseDescriptor;
    UINT32 m_descriptorHandleIncrementSize;
    UINT32 m_numFreeHandles;

    std::mutex m_allocationMutex;
};