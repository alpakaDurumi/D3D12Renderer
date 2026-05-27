#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include <basetsd.h>
#include <d3d12.h>

class CommandQueue;
class DescriptorAllocatorPage;
class DescriptorAllocation;

// DescriptorAllocator class is used to allocate descriptors to the application when loading new resources
class DescriptorAllocator
{
public:
    DescriptorAllocator(const DescriptorAllocator&) = delete;
    DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;
    DescriptorAllocator(DescriptorAllocator&&) = delete;
    DescriptorAllocator& operator=(DescriptorAllocator&&) = delete;

    DescriptorAllocator();
    ~DescriptorAllocator();

    void Init(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type);
    void SetCommandQueue(const CommandQueue* pCommandQueue);

    DescriptorAllocation Allocate(UINT32 numDescriptors = 1);

private:
    static constexpr UINT32 NumDescriptorsPerHeap = 256;

    // These functions not use mutex since they assume that mutex already locked on caller's side.
    // If this function called outside of DescriptorAllocator::Allocate, explicit mutex should be locked.
    DescriptorAllocatorPage* CreateAllocatorPage();
    void ReleaseStaleDescriptors(UINT64 completedFenceValue);

    D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;

    std::vector<std::unique_ptr<DescriptorAllocatorPage>> m_heapPool;
    std::set<SIZE_T> m_availableHeaps; // Indices of available heaps in the heap pool.

    std::mutex m_allocationMutex;

    ID3D12Device* m_pDevice = nullptr;
    const CommandQueue* m_pCommandQueue = nullptr;
};
