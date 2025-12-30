#include "pch.h"
#include "DynamicDescriptorHeap.h"

#include "D3DHelper.h"
#include "RootSignature.h"
#include "CommandQueue.h"

using namespace D3DHelper;

DynamicDescriptorHeap::DynamicDescriptorHeap(const ComPtr<ID3D12Device10>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT32 numDescriptorsPerHeap)
    : m_device(device)
    , m_heapType(heapType)
    , m_numDescriptorsPerHeap(numDescriptorsPerHeap)
    , m_descriptorTableBitMask(0)
    , m_staleDescriptorTableBitMask(0)
    , m_currentCPUDescriptorHandle{ 0 }
    , m_currentGPUDescriptorHandle{ 0 }
    , m_numFreeHandles(0)
    , m_pCommandQueue(nullptr)
    , m_currentOffset(0)
    , m_numParameters(0)
    , m_numStaticSamplers(0)
{
    m_descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(m_heapType);

    // Allocate space for staging CPU visible descriptors
    m_descriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(m_numDescriptorsPerHeap);

    // Allocate first descriptor heap
    m_currentHeap = RequestDescriptorHeap();
    m_currentCPUDescriptorHandle = m_currentHeap->GetCPUDescriptorHandleForHeapStart();
    m_currentGPUDescriptorHandle = m_currentHeap->GetGPUDescriptorHandleForHeapStart();
    m_numFreeHandles = m_numDescriptorsPerHeap;
}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature)
{
    // Reset stale descriptors
    m_staleDescriptorTableBitMask = 0;

    // Get a bit mask that represents the root parameter indices that match the 
    // descriptor heap type for this dynamic descriptor heap.
    m_descriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(m_heapType);

    m_currentOffset = 0;

    m_numParameters = rootSignature.GetNumParameters();
    m_numStaticSamplers = rootSignature.GetNumStaticSamplers();
}

void DynamicDescriptorHeap::StageDescriptors(UINT32 rootParameterIndex, UINT32 offset, UINT32 numDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor)
{
    if (rootParameterIndex >= MaxDescriptorTables)
        throw std::out_of_range("Root parameter index exceeds MaxDescriptorTables.");

    DescriptorTableCache& descriptorTableCache = m_descriptorTableCache[rootParameterIndex];

    // If parameter already staged.
    // Check if range of parameter can extended.
    if (descriptorTableCache.BaseDescriptor)
    {
        if (rootParameterIndex == m_numParameters - 1)
        {
            if ((descriptorTableCache.BaseDescriptor - m_descriptorHandleCache.get() + offset + numDescriptors) > m_numDescriptorsPerHeap)
                throw std::out_of_range("input range exceeds the heap boundary.");

            descriptorTableCache.NumDescriptors = std::max(descriptorTableCache.NumDescriptors, offset + numDescriptors);
            m_currentOffset = static_cast<UINT32>(descriptorTableCache.BaseDescriptor - m_descriptorHandleCache.get()) + descriptorTableCache.NumDescriptors;
        }
        else
        {
            DWORD nextTableIndex;
            UINT16 nextTableMask = m_descriptorTableBitMask & ~((1 << (rootParameterIndex + 1)) - 1);
            if (_BitScanForward(&nextTableIndex, nextTableMask))
            {
                // If next parameter staged, check corruption
                if (m_descriptorTableCache[nextTableIndex].BaseDescriptor)
                {
                    if (descriptorTableCache.BaseDescriptor + offset + numDescriptors > m_descriptorTableCache[nextTableIndex].BaseDescriptor)
                        throw std::out_of_range("Input range corrupts the next table range.");
                }
                // If next parameter not yet staged, check heap boundary
                else
                {
                    if ((descriptorTableCache.BaseDescriptor - m_descriptorHandleCache.get() + offset + numDescriptors) > m_numDescriptorsPerHeap)
                        throw std::out_of_range("Number of descriptors exceeds the heap boundary even if a new heap allocated.");

                    descriptorTableCache.NumDescriptors = std::max(descriptorTableCache.NumDescriptors, offset + numDescriptors);
                    m_currentOffset = static_cast<UINT32>(descriptorTableCache.BaseDescriptor - m_descriptorHandleCache.get()) + descriptorTableCache.NumDescriptors;
                }
            }
            else
            {
                if ((descriptorTableCache.BaseDescriptor - m_descriptorHandleCache.get() + offset + numDescriptors) > m_numDescriptorsPerHeap)
                    throw std::out_of_range("Number of descriptors exceeds the heap boundary even if a new heap allocated.");

                descriptorTableCache.NumDescriptors = std::max(descriptorTableCache.NumDescriptors, offset + numDescriptors);
                m_currentOffset = static_cast<UINT32>(descriptorTableCache.BaseDescriptor - m_descriptorHandleCache.get()) + descriptorTableCache.NumDescriptors;
            }
        }
    }
    // If new parameter staged.
    // Staging new parameters MUST be done in ascending order of parameter index.
    else
    {
        if ((m_currentOffset + offset + numDescriptors) > m_numDescriptorsPerHeap)
            throw std::out_of_range("Number of descriptors exceeds the heap boundary even if a new heap allocated.");

        // Set start address and accumulate number of descriptors
        descriptorTableCache.BaseDescriptor = m_descriptorHandleCache.get() + m_currentOffset;
        descriptorTableCache.NumDescriptors = offset + numDescriptors;
        m_currentOffset += descriptorTableCache.NumDescriptors;
    }

    // Copy descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE* pDest = descriptorTableCache.BaseDescriptor + offset;
    for (UINT32 i = 0; i < numDescriptors; ++i)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE temp = srcDescriptor;
        MoveCPUDescriptorHandle(&temp, i, m_descriptorHandleIncrementSize);
        pDest[i] = temp;
    }

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
        numStaleDescriptors += m_descriptorTableCache[i].NumDescriptors;
        staleDescriptorsBitMask ^= (1 << i);
    }

    return numStaleDescriptors;
}

ID3D12DescriptorHeap* DynamicDescriptorHeap::RequestDescriptorHeap()
{
    while (!m_pendingHeaps.empty() && m_pCommandQueue->IsFenceComplete(m_pendingHeaps.front().first))
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
    heapDesc.NumDescriptors = m_numDescriptorsPerHeap;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap)));

    return descriptorHeap;
}

void DynamicDescriptorHeap::CommitStagedDescriptors(ComPtr<ID3D12GraphicsCommandList7>& commandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> setFunc)
{
    // Compute the number of descriptors that need to be copied 
    UINT32 numDescriptorsToCommit = ComputeStaleDescriptorCount();

    if (numDescriptorsToCommit > 0)
    {
        if (m_numFreeHandles < numDescriptorsToCommit)
        {
            m_retiredHeaps.push_back(m_currentHeap);

            m_currentHeap = RequestDescriptorHeap();
            m_currentCPUDescriptorHandle = m_currentHeap->GetCPUDescriptorHandleForHeapStart();
            m_currentGPUDescriptorHandle = m_currentHeap->GetGPUDescriptorHandleForHeapStart();
            m_numFreeHandles = m_numDescriptorsPerHeap;

            ID3D12DescriptorHeap* ppHeaps[] = { m_currentHeap };
            commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

            // When updating the descriptor heap on the command list, all descriptor
            // tables must be (re)recopied to the new descriptor heap (not just the stale descriptor tables)
            m_staleDescriptorTableBitMask = m_descriptorTableBitMask;
        }

        DWORD rootIndex;
        while (_BitScanForward(&rootIndex, m_staleDescriptorTableBitMask))
        {
            UINT numSrcDescriptors = m_descriptorTableCache[rootIndex].NumDescriptors;
            D3D12_CPU_DESCRIPTOR_HANDLE* pSrcDescriptorHandles = m_descriptorTableCache[rootIndex].BaseDescriptor;

            D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[] = { m_currentCPUDescriptorHandle };
            UINT pDestDescriptorRangeSizes[] = { numSrcDescriptors };

            // Copy the staged CPU visible descriptors to the GPU visible descriptor heap.
            // Assume that descriptors in m_descriptorHandleCache are discontinuous or reside in different heap.
            m_device->CopyDescriptors(
                1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
                numSrcDescriptors, pSrcDescriptorHandles, nullptr,
                m_heapType);

            // Set the descriptors on the command list using the passed-in setter function.
            setFunc(commandList.Get(), rootIndex, m_currentGPUDescriptorHandle);

            // Offset current CPU and GPU descriptor handles.
            MoveCPUAndGPUDescriptorHandle(&m_currentCPUDescriptorHandle, &m_currentGPUDescriptorHandle, numSrcDescriptors, m_descriptorHandleIncrementSize);
            m_numFreeHandles -= numSrcDescriptors;

            // Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor
            m_staleDescriptorTableBitMask ^= (1 << rootIndex);
        }

        // Reset variables
        m_currentOffset = 0;
        for (auto& cache : m_descriptorTableCache)
            cache.Reset();
    }
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDraw(ComPtr<ID3D12GraphicsCommandList7>& commandList)
{
    CommitStagedDescriptors(commandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(ComPtr<ID3D12GraphicsCommandList7>& commandList)
{
    CommitStagedDescriptors(commandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

// Copy a single CPU visible descriptor to a GPU visible descriptor heap
D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::CopyDescriptor(ComPtr<ID3D12GraphicsCommandList7>& commandList, D3D12_CPU_DESCRIPTOR_HANDLE cpuDescriptor)
{
    if (m_numFreeHandles < 1)
    {
        m_retiredHeaps.push_back(m_currentHeap);

        m_currentHeap = RequestDescriptorHeap();
        m_currentCPUDescriptorHandle = m_currentHeap->GetCPUDescriptorHandleForHeapStart();
        m_currentGPUDescriptorHandle = m_currentHeap->GetGPUDescriptorHandleForHeapStart();
        m_numFreeHandles = m_numDescriptorsPerHeap;

        ID3D12DescriptorHeap* ppHeaps[] = { m_currentHeap };
        commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        m_staleDescriptorTableBitMask = m_descriptorTableBitMask;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE hGPU = m_currentGPUDescriptorHandle;
    m_device->CopyDescriptorsSimple(1, m_currentCPUDescriptorHandle, cpuDescriptor, m_heapType);

    MoveCPUAndGPUDescriptorHandle(&m_currentCPUDescriptorHandle, &m_currentGPUDescriptorHandle, 1, m_descriptorHandleIncrementSize);
    m_numFreeHandles -= 1;

    return hGPU;
}

ID3D12DescriptorHeap* DynamicDescriptorHeap::GetCurrentDescriptorHeap() const
{
    assert(m_currentHeap != nullptr);
    return m_currentHeap;
}

void DynamicDescriptorHeap::QueueRetiredHeaps(UINT64 fenceValue)
{
    for (auto* page : m_retiredHeaps)
    {
        m_pendingHeaps.push({ fenceValue, page });
    }
    m_retiredHeaps.clear();
}