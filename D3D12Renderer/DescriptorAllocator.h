#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include <memory>
#include <mutex>
#include <set>
#include <vector>

#include "DescriptorAllocatorPage.h"
#include "DescriptorAllocation.h"

class CommandQueue;

using Microsoft::WRL::ComPtr;

// DescriptorAllocator class is used to allocate descriptors to the application when loading new resources
class DescriptorAllocator
{
public:
    // Disable copy and move. Only use as l-value reference
    DescriptorAllocator(const DescriptorAllocator&) = delete;
    DescriptorAllocator& operator=(const DescriptorAllocator&) = delete;
    DescriptorAllocator(DescriptorAllocator&&) = delete;
    DescriptorAllocator& operator=(DescriptorAllocator&&) = delete;

    DescriptorAllocator(const ComPtr<ID3D12Device10>& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT32 numDescriptorsPerHeap = 256);

    DescriptorAllocation Allocate(UINT32 numDescriptors = 1);

    void SetCommandQueue(const CommandQueue* pCommandQueue) { m_pCommandQueue = pCommandQueue; }

private:
    // These functions not use mutex since they assume that mutex already locked on caller's side.
    // If this function called outside of DescriptorAllocator::Allocate, explicit mutex should be locked.
    DescriptorAllocatorPage* CreateAllocatorPage();
    void ReleaseStaleDescriptors(UINT64 completedFenceValue);

    D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
    UINT32 m_numDescriptorsPerHeap;

    std::vector<std::unique_ptr<DescriptorAllocatorPage>> m_heapPool;
    std::set<SIZE_T> m_availableHeaps;      // Indices of available heaps in the heap pool.

    std::mutex m_allocationMutex;

    ComPtr<ID3D12Device10> m_device;
    const CommandQueue* m_pCommandQueue;
};