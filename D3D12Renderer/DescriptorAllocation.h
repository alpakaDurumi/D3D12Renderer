#pragma once

#include <wrl/client.h>
#include <d3d12.h>

#include <memory>

class DescriptorAllocatorPage;

// move-only self-freeing type that is used as a wrapper for a D3D12_CPU_DESCRIPTOR_HANDLE
// The reason why the DescriptorAllocation must be a move-only class is to ensure there is only a single instance of a particular allocation
class DescriptorAllocation
{
public:
    DescriptorAllocation() = delete;
    DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor, UINT32 offsetInHeap, UINT32 numHandles, UINT32 descriptorSize, std::shared_ptr<DescriptorAllocatorPage> page);

    ~DescriptorAllocation();

    // Copies are not allowed
    DescriptorAllocation(const DescriptorAllocation&) = delete;
    DescriptorAllocation& operator=(const DescriptorAllocation&) = delete;

    // Only move is allowed
    DescriptorAllocation(DescriptorAllocation&& other);
    DescriptorAllocation& operator=(DescriptorAllocation&& other);

    // Get a descriptor at a particular offset in the allocation
    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle(UINT32 offsetInBlock = 0) const;

    UINT32 GetOffset() const { return m_offsetInHeap; }
    UINT32 GetNumHandles() const { return m_numHandles; }
    UINT64 GetFenceValue() const { return m_fenceValue; }
    std::shared_ptr<DescriptorAllocatorPage> GetDescriptorAllocatorPage() const { return m_page; }

private:
    void Free();

    D3D12_CPU_DESCRIPTOR_HANDLE m_descriptor;
    UINT32 m_offsetInHeap;
    UINT32 m_numHandles;
    UINT32 m_descriptorSize;
    UINT64 m_fenceValue;

    std::shared_ptr<DescriptorAllocatorPage> m_page;
};