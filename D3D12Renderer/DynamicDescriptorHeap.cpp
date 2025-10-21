#include "DynamicDescriptorHeap.h"
#include <cassert>
#include <stdexcept>

#include "D3DHelper.h"

#include "RootSignature.h"

using namespace D3DHelper;

DynamicDescriptorHeap::DynamicDescriptorHeap(ComPtr<ID3D12Device>& device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT32 numDescriptorsPerHeap)
    : m_device(device)
    , m_heapType(heapType)
    , m_numDescriptorsPerHeap(numDescriptorsPerHeap)
    , m_descriptorTableBitMask(0)
    , m_staleDescriptorTableBitMask(0)
    , m_currentCPUDescriptorHandle{ 0 }
    , m_currentGPUDescriptorHandle{ 0 }
    , m_numFreeHandles(0)
{
    m_descriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize(m_heapType);

    // Allocate space for staging CPU visible descriptors
    m_descriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(m_numDescriptorsPerHeap);
}

// 본인 heapType에 따라 루트시그니처에서 어떤 비트마스크를 보고 처리할 지 결정해야한다.
// CBV_SRV_UAV 힙인지 또는 Sampler 힙인지에 따라 RootSignature에서 처리할 비트마스크를 선택하자.
void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& rootSignature)
{
    // Reset stale descriptors
    m_staleDescriptorTableBitMask = 0;

    // Get a bit mask that represents the root parameter indices that match the 
    // descriptor heap type for this dynamic descriptor heap
    m_descriptorTableBitMask = rootSignature.GetDescriptorTableBitMask(m_heapType);
    UINT32 descriptorTableBitMask = m_descriptorTableBitMask;

    UINT32 currentOffset = 0;
    DWORD rootIndex;
    while (_BitScanForward(&rootIndex, descriptorTableBitMask) && rootIndex < rootSignature.GetNumParameters())
    {
        UINT32 numDescriptors = rootSignature.GetTableSize(rootIndex);

        DescriptorTableCache& descriptorTableCache = m_descriptorTableCache[rootIndex];
        descriptorTableCache.NumDescriptors = numDescriptors;
        // rootIndex번째 table에 대힌 디스크립터가 m_descriptorHandleCache 내에서 저장될 위치를 지정
        descriptorTableCache.BaseDescriptor = m_descriptorHandleCache.get() + currentOffset;

        currentOffset += numDescriptors;

        // Flip the descriptor table bit so it's not scanned again for the current index.
        descriptorTableBitMask ^= (1 << rootIndex);
    }

    // Make sure the maximum number of descriptors per descriptor heap has not been exceeded.
    assert(currentOffset <= m_numDescriptorsPerHeap);
}

void DynamicDescriptorHeap::StageDescriptors(UINT32 rootParameterIndex, UINT32 offset, UINT32 numDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE srcDescriptor)
{
    // Cannot stage more than MaxDescriptorTables root parameters
    if (rootParameterIndex >= MaxDescriptorTables)
    {
        throw std::out_of_range("Root parameter index exceeds MaxDescriptorTables.");
    }

    DescriptorTableCache& descriptorTableCache = m_descriptorTableCache[rootParameterIndex];

    // Check that the number of descriptors to copy does not exceed the number
    // of descriptors expected in the descriptor table.
    if ((offset + numDescriptors) > descriptorTableCache.NumDescriptors)
    {
        throw std::length_error("Number of descriptors exceeds the number of descriptors in the descriptor table.");
    }

    // Copy descriptor handles
    D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = (descriptorTableCache.BaseDescriptor + offset);
    for (UINT32 i = 0; i < numDescriptors; ++i)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE temp = srcDescriptor;
        MoveCPUDescriptorHandle(&temp, i, m_descriptorHandleIncrementSize);
        dstDescriptor[i] = temp;
    }

    // Set the root parameter index bit to make sure the descriptor table 
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

ComPtr<ID3D12DescriptorHeap> DynamicDescriptorHeap::RequestDescriptorHeap()
{
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    if (!m_availableDescriptorHeaps.empty())
    {
        descriptorHeap = m_availableDescriptorHeaps.front();
        m_availableDescriptorHeaps.pop();
    }
    else
    {
        descriptorHeap = CreateDescriptorHeap();
        m_descriptorHeapPool.push(descriptorHeap);
    }

    return descriptorHeap;
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
        if (!m_currentDescriptorHeap || m_numFreeHandles < numDescriptorsToCommit)
        {
            m_currentDescriptorHeap = RequestDescriptorHeap();
            m_currentCPUDescriptorHandle = m_currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
            m_currentGPUDescriptorHandle = m_currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
            m_numFreeHandles = m_numDescriptorsPerHeap;

            ID3D12DescriptorHeap* ppHeaps[] = { m_currentDescriptorHeap.Get() };
            commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

            // When updating the descriptor heap on the command list, all descriptor
            // tables must be (re)recopied to the new descriptor heap (not just
            // the stale descriptor tables)
            // GPU에 제출할 디스크립터 힙이 바뀌면, 디스크립터가 변경되지 않았더라도 다시 해당 힙으로 복사해줘야 한다.
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
            // 상당히 이상하다. 수정이 필요할 것 같다. src에 대한 인자 3개를 (1, pSrcDescriptorHandles, pDestDescriptorRangeSizes)로 바꿔도 될 듯
            m_device->CopyDescriptors(1, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
                numSrcDescriptors, pSrcDescriptorHandles, nullptr, m_heapType);
            // Set the descriptors on the command list using the passed-in setter function.
            setFunc(commandList.Get(), rootIndex, m_currentGPUDescriptorHandle);

            // Offset current CPU and GPU descriptor handles.
            MoveCPUAndGPUDescriptorHandle(&m_currentCPUDescriptorHandle, &m_currentGPUDescriptorHandle, numSrcDescriptors, m_descriptorHandleIncrementSize);
            m_numFreeHandles -= numSrcDescriptors;

            // Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor
            m_staleDescriptorTableBitMask ^= (1 << rootIndex);
        }
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
    if (!m_currentDescriptorHeap || m_numFreeHandles < 1)
    {
        m_currentDescriptorHeap = RequestDescriptorHeap();
        m_currentCPUDescriptorHandle = m_currentDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        m_currentGPUDescriptorHandle = m_currentDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        m_numFreeHandles = m_numDescriptorsPerHeap;

        ID3D12DescriptorHeap* ppHeaps[] = { m_currentDescriptorHeap.Get() };
        commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        m_staleDescriptorTableBitMask = m_descriptorTableBitMask;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE hGPU = m_currentGPUDescriptorHandle;
    m_device->CopyDescriptorsSimple(1, m_currentCPUDescriptorHandle, cpuDescriptor, m_heapType);

    MoveCPUAndGPUDescriptorHandle(&m_currentCPUDescriptorHandle, &m_currentGPUDescriptorHandle, 1, m_descriptorHandleIncrementSize);
    m_numFreeHandles -= 1;

    return hGPU;
}

// 현재 DynamicDescriptorHeap은 각 인스턴스가 단일 커맨드 리스트에 의해 사용된다고 가정하고 있음
// 멀티스레드 버전으로 수정하게 된다면 이 함수는 아마 쓰지 않게 될 것임
// Fence 기반의 모델로 수정해야 함
void DynamicDescriptorHeap::Reset()
{
    m_availableDescriptorHeaps = m_descriptorHeapPool;
    m_currentDescriptorHeap.Reset();
    m_currentCPUDescriptorHandle = { 0 };
    m_currentGPUDescriptorHandle = { 0 };
    m_numFreeHandles = 0;
    m_descriptorTableBitMask = 0;
    m_staleDescriptorTableBitMask = 0;

    // Reset the table cache
    for (int i = 0; i < MaxDescriptorTables; ++i)
    {
        m_descriptorTableCache[i].Reset();
    }
}