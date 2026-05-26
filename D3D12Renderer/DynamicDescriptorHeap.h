#pragma once

#include <array>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

#include <basetsd.h>
#include <d3d12.h>
#include <minwindef.h>
#include <wrl/client.h>

class RootSignature;

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

    DynamicDescriptorHeap() = default;
    ~DynamicDescriptorHeap() = default;

    void Init(ID3D12Device10* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE heapType);

    void StageDescriptors(UINT32 rootParameterIndex, UINT32 offsetInParameter, UINT32 numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE baseCpuHandle);

    bool CheckHeapChanged();

    void CommitStagedDescriptorsForDraw(ID3D12GraphicsCommandList7* pCommandList);
    void CommitStagedDescriptorsForDispatch(ID3D12GraphicsCommandList7* pCommandList);

    D3D12_GPU_DESCRIPTOR_HANDLE CopyDescriptor(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7>& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor);

    void ParseRootSignature(const RootSignature& rootSignature);

    ID3D12DescriptorHeap* GetCurrentDescriptorHeap() const;

    void QueueRetiredHeaps(UINT64 fenceValue);
    void UpdateCompletedFenceValue(UINT64 completedFenceValue);

    void Reset();

private:
    // A 16-bit mask is used to keep track of the root parameter indices that are descriptor tables
    static constexpr UINT32 MaxDescriptorTables = 16;
    static constexpr UINT32 NumDescriptorsPerHeap = 1024;

    void CommitStagedDescriptors(ID3D12GraphicsCommandList7* pCommandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc);

    ID3D12DescriptorHeap* RequestDescriptorHeap();
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap();

    UINT32 ComputeStaleDescriptorCount() const;

    // A structure that represents a descriptor table entry in the root signature.
    struct DescriptorTableEntry
    {
        bool IsEmpty() const
        {
            return Offset == UINT_MAX;
        }

        UINT Offset = UINT_MAX;
        UINT32 NumDescriptors = 0;
    };

    D3D12_DESCRIPTOR_HEAP_TYPE m_heapType;
    UINT32 m_descriptorHandleIncrementSize = 0;

    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, NumDescriptorsPerHeap> m_descriptorHandleCache; // Flat storage for all staged CPU descriptor handles
    DescriptorTableEntry m_descriptorTableEntries[MaxDescriptorTables];                     // Per-rootParameter metadata: start position and count within m_descriptorHandleCache
    UINT32 m_currentOffset = 0;
    UINT m_numParameters = 0;

    // Represents the index in the root signature that contains a descriptor table
    UINT16 m_descriptorTableBitMask = 0;
    // Represents a descriptor table in the root signature that has changed since the last time the descriptors were copied
    UINT16 m_staleDescriptorTableBitMask = 0;

    std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> m_heapPool;
    std::queue<ID3D12DescriptorHeap*> m_availableHeaps;

    std::vector<ID3D12DescriptorHeap*> m_retiredHeaps;
    std::queue<std::pair<UINT64, ID3D12DescriptorHeap*>> m_pendingHeaps;

    ID3D12DescriptorHeap* m_currentHeap = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE m_currentGpuDescriptorHandle;
    D3D12_CPU_DESCRIPTOR_HANDLE m_currentCpuDescriptorHandle;
    UINT32 m_numFreeHandles; // Number of free handles in current descriptor heap

    UINT64 m_completedFenceValue = 0;

    ID3D12Device10* m_pDevice = nullptr;
};
