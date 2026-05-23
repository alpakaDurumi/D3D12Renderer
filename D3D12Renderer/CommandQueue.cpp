#include "pch.h"
#include "CommandQueue.h"

#include "D3DHelper.h"
#include "DynamicDescriptorHeap.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

CommandQueue::CommandQueue(const ComPtr<ID3D12Device10>& device, D3D12_COMMAND_LIST_TYPE type)
    : m_type(type), m_fenceValue(0), m_device(device), m_pDynamicDescriptorHeapForCbvSrvUav(nullptr), m_pSamplerDescriptorHeap(nullptr)
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = type;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    ThrowIfFailed(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    ThrowIfFailed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

    m_fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr)
    {
        ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }
}

CommandQueue::~CommandQueue()
{
    CloseHandle(m_fenceEvent);
}

void CommandQueue::SetDescriptorHeaps(const DynamicDescriptorHeap* pHeapForCbvSrvUav, ID3D12DescriptorHeap* pHeapForSampler)
{
    m_pDynamicDescriptorHeapForCbvSrvUav = pHeapForCbvSrvUav;
    m_pSamplerDescriptorHeap = pHeapForSampler;
}

ID3D12CommandQueue* CommandQueue::GetCommandQueue() const
{
    return m_commandQueue.Get();
}

ID3D12CommandAllocator* CommandQueue::CreateCommandAllocator()
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(m_device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&commandAllocator)));
    m_commandAllocatorPool.push_back(commandAllocator);

    return commandAllocator.Get();
}

ID3D12GraphicsCommandList7* CommandQueue::CreateCommandList(ID3D12CommandAllocator* pCommandAllocator)
{
    ComPtr<ID3D12GraphicsCommandList7> commandList;
    ThrowIfFailed(m_device->CreateCommandList(0, m_type, pCommandAllocator, nullptr, IID_PPV_ARGS(&commandList)));
    m_commandListPool.push_back(commandList);

    return commandList.Get();
}

std::pair<ID3D12CommandAllocator*, ID3D12GraphicsCommandList7*> CommandQueue::GetAvailableCommandList()
{
    ID3D12CommandAllocator* pCommandAllocator;
    ID3D12GraphicsCommandList7* pCommandList;

    // Command allocator queue에 GPU 작업이 끝난 allocator가 존재한다면 그것을 사용하고 없다면 새로 생성
    if (!m_commandAllocatorQueue.empty() && IsFenceComplete(m_commandAllocatorQueue.front().fenceValue))
    {
        pCommandAllocator = m_commandAllocatorQueue.front().pCommandAllocator;
        m_commandAllocatorQueue.pop();
        ThrowIfFailed(pCommandAllocator->Reset());
    }
    else
    {
        pCommandAllocator = CreateCommandAllocator();
    }

    // Command list는 execute만 되면 즉시 재사용이 가능하므로 큐에 있으면 바로 사용하고 없으면 새로 생성
    // 직전에 얻은 commandAllocator에 연결
    if (!m_commandListQueue.empty())
    {
        pCommandList = m_commandListQueue.front();
        m_commandListQueue.pop();
        ThrowIfFailed(pCommandList->Reset(pCommandAllocator, nullptr));
    }
    else
    {
        pCommandList = CreateCommandList(pCommandAllocator);
    }

    // Bind with DescriptorHeaps
    ID3D12DescriptorHeap* ppHeaps[] = { m_pDynamicDescriptorHeapForCbvSrvUav->GetCurrentDescriptorHeap(), m_pSamplerDescriptorHeap };
    pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    return { pCommandAllocator, pCommandList };
}

UINT64 CommandQueue::ExecuteCommandLists(ID3D12CommandAllocator* pCommandAllocator, ID3D12GraphicsCommandList7* pCommandList)
{
    pCommandList->Close();

    ID3D12CommandList* ppCommandLists[] = { pCommandList };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    UINT64 fenceValue = Signal();
    m_commandAllocatorQueue.push({ fenceValue, pCommandAllocator });
    m_commandListQueue.push(pCommandList);

    return fenceValue;
}

UINT64 CommandQueue::Signal()
{
    m_fenceValue++;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));
    return m_fenceValue;
}

UINT64 CommandQueue::GetCompletedFenceValue() const
{
    return m_fence->GetCompletedValue();
}

bool CommandQueue::IsFenceComplete(UINT64 fenceValue) const
{
    return m_fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(UINT64 fenceValue)
{
    if (!IsFenceComplete(fenceValue))
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }
}

// Wait for pending GPU work to complete
void CommandQueue::Flush()
{
    UINT64 fenceValue = Signal();
    WaitForFenceValue(fenceValue);
}