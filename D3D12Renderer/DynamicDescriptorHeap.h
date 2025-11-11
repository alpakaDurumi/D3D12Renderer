#pragma once

#include <wrl/client.h>

#include <d3d12.h>

#include <memory>
#include <queue>
#include <functional>

class RootSignature;

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

    void StageDescriptors(UINT32 rootParameterIndex, UINT32 offset, UINT32 numDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptors);

    // Copy all of the staged descriptors to the GPU visible descriptor heap and
    // bind the descriptor heap and the descriptor tables to the command list
    
    void CommitStagedDescriptorsForDraw(ComPtr<ID3D12GraphicsCommandList7>& commandList);
    void CommitStagedDescriptorsForDispatch(ComPtr<ID3D12GraphicsCommandList7>& commandList);

    D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(ComPtr<ID3D12GraphicsCommandList7>& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);

    void ParseRootSignature(const RootSignature& rootSignature);

    ComPtr<ID3D12DescriptorHeap> GetCurrentDescriptorHeap();

private:
    void CommitStagedDescriptors(ComPtr<ID3D12GraphicsCommandList7>& commandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);

    ComPtr<ID3D12DescriptorHeap> RequestDescriptorHeap();
    ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap();

    UINT32 ComputeStaleDescriptorCount() const;

    // A 32-bit mask is used to keep track of the root parameter indices that are descriptor tables
    static const UINT32 MaxDescriptorTables = 32;

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

    // Represents the index in the root signature that contains a descriptor table
    UINT32 m_descriptorTableBitMask;
    // Represents a descriptor table in the root signature that has changed since the last time the 
    // descriptors were copied.
    UINT32 m_staleDescriptorTableBitMask;

    using DescriptorHeapPool = std::queue<ComPtr<ID3D12DescriptorHeap>>;

    DescriptorHeapPool m_descriptorHeapPool;
    DescriptorHeapPool m_availableDescriptorHeaps;

    ComPtr<ID3D12DescriptorHeap> m_currentDescriptorHeap;
    D3D12_GPU_DESCRIPTOR_HANDLE m_currentGPUDescriptorHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_currentCPUDescriptorHandle;
    UINT32 m_numFreeHandles;    // Number of free handles in current descriptor heap

    ComPtr<ID3D12Device10> m_device;
};