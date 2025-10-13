#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <memory>
#include <mutex>
#include <set>
#include <vector>

class DescriptorAllocatorPage;
class DescriptorAllocation;

using Microsoft::WRL::ComPtr;

// DescriptorAllocator class is used to allocate descriptors to the application when loading new resources
class DescriptorAllocator
{
public:
    DescriptorAllocator(ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT32 numDescriptorsPerHeap = 256);

    DescriptorAllocation Allocate(UINT32 numDescriptors = 1);
    void ReleaseStaleDescriptors(UINT64 completedFenceValue);

private:
    // Create a new heap with a specific number of descriptors
    std::shared_ptr<DescriptorAllocatorPage> CreateAllocatorPage();

    D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
    UINT32 m_numDescriptorsPerHeap;

    std::vector<std::shared_ptr<DescriptorAllocatorPage>> m_heapPool;
    // Indices of available heaps in the heap pool.
    std::set<SIZE_T> m_availableHeaps;

    std::mutex m_allocationMutex;

    ComPtr<ID3D12Device> m_device;
};