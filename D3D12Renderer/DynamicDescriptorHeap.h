#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include <memory>
#include <queue>
#include <functional>
#include <unordered_set>

#include "DescriptorAllocation.h"

class RootSignature;
class CommandQueue;

using Microsoft::WRL::ComPtr;

// Staging CPU visible descriptors and committing those descriptors to a GPU visible descriptor heap
// Root constants and root descriptors do not use descriptor heap
// For multithreading, this class should be modified to use mutex
class DynamicDescriptorHeap
{
public:
    // Disable copy and move. Only use as l-value reference
    DynamicDescriptorHeap(const DynamicDescriptorHeap&) = delete;
    DynamicDescriptorHeap& operator=(const DynamicDescriptorHeap&) = delete;
    DynamicDescriptorHeap(DynamicDescriptorHeap&&) = delete;
    DynamicDescriptorHeap& operator=(DynamicDescriptorHeap&&) = delete;

    DynamicDescriptorHeap(const ComPtr<ID3D12Device10>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT32 numDescriptorsPerHeap = 1024);

    void SetCommandQueue(const CommandQueue* pCommandQueue) { m_pCommandQueue = pCommandQueue; }

    void StageDescriptors(UINT32 rootParameterIndex, UINT32 offsetInParameter, UINT32 numDescriptors, DescriptorAllocation& allocation, UINT32 offsetInAllocation = 0);

    // Copy all of the staged descriptors to the GPU visible descriptor heap and
    // bind the descriptor heap and the descriptor tables to the command list
    
    void CommitStagedDescriptorsForDraw(ComPtr<ID3D12GraphicsCommandList7>& commandList);
    void CommitStagedDescriptorsForDispatch(ComPtr<ID3D12GraphicsCommandList7>& commandList);

    D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(ComPtr<ID3D12GraphicsCommandList7>& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);

    void ParseRootSignature(const RootSignature& rootSignature);

    ID3D12DescriptorHeap* GetCurrentDescriptorHeap() const;

    void QueueRetiredHeaps(UINT64 fenceValue);

    void Reset();

private:
    void CommitStagedDescriptors(ComPtr<ID3D12GraphicsCommandList7>& commandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);

    ID3D12DescriptorHeap* RequestDescriptorHeap();
    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap();

    UINT32 ComputeStaleDescriptorCount() const;

    // A 16-bit mask is used to keep track of the root parameter indices that are descriptor tables
    static const UINT32 MaxDescriptorTables = 16;

    // A structure that represents a descriptor table entry in the root signature.
    struct DescriptorTableCache
    {
        DescriptorTableCache()
            : NumDescriptors(0), BaseDescriptor(nullptr)
        {
        }

        void Reset()
        {
            NumDescriptors = 0;
            BaseDescriptor = nullptr;
        }

        UINT32 NumDescriptors;
        D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor;
    };

    D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
    UINT32 m_numDescriptorsPerHeap;
    UINT32 m_descriptorHandleIncrementSize;

    std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> m_descriptorHandleCache;
    DescriptorTableCache m_descriptorTableCache[MaxDescriptorTables];

    UINT32 m_currentOffset;

    UINT m_numParameters;
    UINT m_numStaticSamplers;

    // Represents the index in the root signature that contains a descriptor table
    UINT16 m_descriptorTableBitMask;
    // Represents a descriptor table in the root signature that has changed since the last time the 
    // descriptors were copied.
    UINT16 m_staleDescriptorTableBitMask;

    std::vector<ComPtr<ID3D12DescriptorHeap>> m_heapPool;
    std::queue<ID3D12DescriptorHeap*> m_availableHeaps;

    std::vector<ID3D12DescriptorHeap*> m_retiredHeaps;
    std::queue<std::pair<UINT64, ID3D12DescriptorHeap*>> m_pendingHeaps;

    ID3D12DescriptorHeap* m_currentHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_currentGPUDescriptorHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_currentCPUDescriptorHandle;
    UINT32 m_numFreeHandles;    // Number of free handles in current descriptor heap

    std::unordered_set<DescriptorAllocation*> m_usedAllocations;

    ComPtr<ID3D12Device10> m_device;
    const CommandQueue* m_pCommandQueue;     // For IsFenceComplete
};