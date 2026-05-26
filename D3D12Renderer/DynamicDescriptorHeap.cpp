#include "pch.h"

#include "DynamicDescriptorHeap.h"

#include "D3DHelper.h"
#include "RootSignature.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

void DynamicDescriptorHeap::Init(ID3D12Device10* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
    m_pDevice = pDevice;
    m_heapType = heapType;
    m_descriptorHandleIncrementSize = pDevice->GetDescriptorHandleIncrementSize(m_heapType);

    // Allocate first descriptor heap
    m_currentHeap = RequestDescriptorHeap();
    m_currentCpuDescriptorHandle = m_currentHeap->GetCPUDescriptorHandleForHeapStart();
    m_currentGpuDescriptorHandle = m_currentHeap->GetGPUDescriptorHandleForHeapStart();
    m_numFreeHandles = NumDescriptorsPerHeap;
}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature)
{
    // Get a bit mask that represents the root parameter indices that match the
    // descriptor heap type for this dynamic descriptor heap.
    m_descriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(m_heapType);
    // Reset stale descriptors
    m_staleDescriptorTableBitMask = 0;

    m_currentOffset = 0;
    m_numParameters = rootSignature.GetNumParameters();
}

// Staging new parameters MUST be done in ascending order of parameter index.
void DynamicDescriptorHeap::StageDescriptors(UINT32 rootParameterIndex, UINT32 offsetInParameter, UINT32 numDescriptors, D3D12_CPU_DESCRIPTOR_HANDLE baseCpuHandle)
{
    if (rootParameterIndex >= MaxDescriptorTables)
        throw std::out_of_range("Root parameter index exceeds MaxDescriptorTables.");

    DescriptorTableEntry& entry = m_descriptorTableEntries[rootParameterIndex];

    bool isNewParameter = entry.IsEmpty();
    bool isLastStaged = true;

    UINT start = isNewParameter ? m_currentOffset + offsetInParameter : entry.Offset + offsetInParameter;
    UINT upperLimit = NumDescriptorsPerHeap;

    // If given index is not last, check that next table is staged
    if (rootParameterIndex < m_numParameters - 1)
    {
        DWORD nextTableIndex;
        UINT16 nextTableMask = m_descriptorTableBitMask & ~((1 << (rootParameterIndex + 1)) - 1);
        if (_BitScanForward(&nextTableIndex, nextTableMask) && !m_descriptorTableEntries[nextTableIndex].IsEmpty())
        {
            upperLimit = m_descriptorTableEntries[nextTableIndex].Offset;
            isLastStaged = false;
        }
    }

    if ((start + numDescriptors) > upperLimit)
    {
        if (isLastStaged)
            throw std::out_of_range("Input range exceeds the heap boundary even if a new heap allocated.");
        else
            throw std::out_of_range("Input range corrupted the next table range.");
    }

    if (isNewParameter)
    {
        entry.Offset = m_currentOffset;
        entry.NumDescriptors = offsetInParameter + numDescriptors;
        m_currentOffset += entry.NumDescriptors;
    }
    else if (isLastStaged)
    {
        entry.NumDescriptors = std::max(entry.NumDescriptors, offsetInParameter + numDescriptors);
        m_currentOffset = entry.Offset + entry.NumDescriptors;
    }

    // Copy descriptor handles
    for (UINT i = 0; i < numDescriptors; ++i)
        m_descriptorHandleCache[start + i] = GetCpuDescriptorHandle(baseCpuHandle, i, m_descriptorHandleIncrementSize);

    // Set the root parameter index bit as 1 to make sure the descriptor table
    // at that index is bound to the command list.
    m_staleDescriptorTableBitMask |= (1 << rootParameterIndex);
}

// Compute number of descriptors that need to be committed to the GPU visible descriptor heap
UINT32 DynamicDescriptorHeap::ComputeStaleDescriptorCount() const
{
    UINT32 numStaleDescriptors = 0;
    DWORD i;
    DWORD staleDescriptorsBitMask = m_staleDescriptorTableBitMask;

    while (_BitScanForward(&i, staleDescriptorsBitMask))
    {
        numStaleDescriptors += m_descriptorTableEntries[i].NumDescriptors;
        staleDescriptorsBitMask ^= (1 << i);
    }

    return numStaleDescriptors;
}

ID3D12DescriptorHeap* DynamicDescriptorHeap::RequestDescriptorHeap()
{
    while (!m_pendingHeaps.empty() && m_pendingHeaps.front().first <= m_completedFenceValue)
    {
        m_availableHeaps.push(m_pendingHeaps.front().second);
        m_pendingHeaps.pop();
    }

    ID3D12DescriptorHeap* pDescriptorHeap;
    if (!m_availableHeaps.empty())
    {
        pDescriptorHeap = m_availableHeaps.front();
        m_availableHeaps.pop();
    }
    else
    {
        auto descriptorHeap = CreateDescriptorHeap();
        pDescriptorHeap = descriptorHeap.Get();
        m_heapPool.push_back(descriptorHeap);
    }

    return pDescriptorHeap;
}

ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = m_heapType;
    heapDesc.NumDescriptors = NumDescriptorsPerHeap;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

bool DynamicDescriptorHeap::CheckHeapChanged()
{
    // Compute the number of descriptors that need to be copied
    UINT32 numDescriptorsToCommit = ComputeStaleDescriptorCount();

    if (numDescriptorsToCommit > 0)
    {
        if (m_numFreeHandles < numDescriptorsToCommit)
        {
            m_retiredHeaps.push_back(m_currentHeap);

            m_currentHeap = RequestDescriptorHeap();
            m_currentCpuDescriptorHandle = m_currentHeap->GetCPUDescriptorHandleForHeapStart();
            m_currentGpuDescriptorHandle = m_currentHeap->GetGPUDescriptorHandleForHeapStart();
            m_numFreeHandles = NumDescriptorsPerHeap;

            // When updating the descriptor heap on the command list, all descriptor
            // tables must be (re)recopied to the new descriptor heap (not just the stale descriptor tables)
            m_staleDescriptorTableBitMask = m_descriptorTableBitMask;

            return true;
        }
    }

    return false;
}

// Copy all of the staged descriptors to the GPU visible descriptor heap and
// bind the descriptor heap and the descriptor tables to the command list
void DynamicDescriptorHeap::CommitStagedDescriptors(ID3D12GraphicsCommandList7* pCommandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc)
{
    DWORD rootIndex;
    while (_BitScanForward(&rootIndex, m_staleDescriptorTableBitMask))
    {
        UINT numSrcDescriptors = m_descriptorTableEntries[rootIndex].NumDescriptors;
        D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorHandles = &m_descriptorHandleCache[m_descriptorTableEntries[rootIndex].Offset];

        D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[] = {m_currentCpuDescriptorHandle};
        UINT pDestDescriptorRangeSizes[] = {numSrcDescriptors};

        // Copy the staged CPU visible descriptors to the GPU visible descriptor heap.
        // Assume that descriptors in m_descriptorHandleCache are discontinuous or reside in different heap.
        m_pDevice->CopyDescriptors(
            1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
            numSrcDescriptors, pSrcDescriptorHandles, nullptr,
            m_heapType);

        // Set the descriptors on the command list using the passed-in setter function.
        setFunc(pCommandList, rootIndex, m_currentGpuDescriptorHandle);

        // Offset current CPU and GPU descriptor handles.
        m_currentCpuDescriptorHandle = GetCpuDescriptorHandle(m_currentCpuDescriptorHandle, numSrcDescriptors, m_descriptorHandleIncrementSize);
        m_currentGpuDescriptorHandle = GetGpuDescriptorHandle(m_currentGpuDescriptorHandle, numSrcDescriptors, m_descriptorHandleIncrementSize);
        m_numFreeHandles -= numSrcDescriptors;

        // Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor
        m_staleDescriptorTableBitMask ^= (1 << rootIndex);
    }
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDraw(ID3D12GraphicsCommandList7* pCommandList)
{
    CommitStagedDescriptors(pCommandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(ID3D12GraphicsCommandList7* pCommandList)
{
    CommitStagedDescriptors(pCommandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

// Copy a single CPU visible descriptor to a GPU visible descriptor heap
D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(ComPtr<ID3D12GraphicsCommandList7>& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
    if (m_numFreeHandles < 1)
    {
        m_retiredHeaps.push_back(m_currentHeap);

        m_currentHeap = RequestDescriptorHeap();
        m_currentCpuDescriptorHandle = m_currentHeap->GetCPUDescriptorHandleForHeapStart();
        m_currentGpuDescriptorHandle = m_currentHeap->GetGPUDescriptorHandleForHeapStart();
        m_numFreeHandles = NumDescriptorsPerHeap;

        ID3D12DescriptorHeap* ppHeaps[] = {m_currentHeap};
        commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        m_staleDescriptorTableBitMask = m_descriptorTableBitMask;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE hGpu = m_currentGpuDescriptorHandle;
    m_pDevice->CopyDescriptorsSimple(1, m_currentCpuDescriptorHandle, cpuDescriptor, m_heapType);

    m_currentCpuDescriptorHandle = GetCpuDescriptorHandle(m_currentCpuDescriptorHandle, 1, m_descriptorHandleIncrementSize);
    m_currentGpuDescriptorHandle = GetGpuDescriptorHandle(m_currentGpuDescriptorHandle, 1, m_descriptorHandleIncrementSize);
    m_numFreeHandles -= 1;

    return hGpu;
}

ID3D12DescriptorHeap* DynamicDescriptorHeap::GetCurrentDescriptorHeap() const
{
    assert(m_currentHeap != nullptr);
    return m_currentHeap;
}

void DynamicDescriptorHeap::QueueRetiredHeaps(UINT64 fenceValue)
{
    for (auto* pPage : m_retiredHeaps)
    {
        m_pendingHeaps.push({fenceValue, pPage});
    }
    m_retiredHeaps.clear();
}

void DynamicDescriptorHeap::UpdateCompletedFenceValue(UINT64 completedFenceValue)
{
    m_completedFenceValue = completedFenceValue;
}

void DynamicDescriptorHeap::Reset()
{
    m_currentOffset = 0;
    for (auto& entry : m_descriptorTableEntries)
    {
        entry.Offset = UINT_MAX;
        entry.NumDescriptors = 0;
    }
}
