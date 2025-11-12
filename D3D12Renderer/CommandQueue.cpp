#include "pch.h"
#include "CommandQueue.h"

#include "D3DHelper.h"
#include "ResourceLayoutTracker.h"
#include "DynamicDescriptorHeap.h"

using namespace D3DHelper;

CommandQueue::CommandQueue(const ComPtr<ID3D12Device10>& device, DynamicDescriptorHeap& dynamicDescriptorHeap, D3D12_COMMAND_LIST_TYPE type)
    : m_type(type), m_fenceValue(0), m_device(device), m_dynamicDescriptorHeap(dynamicDescriptorHeap)
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

ComPtr<ID3D12CommandAllocator> CommandQueue::CreateCommandAllocator()
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ThrowIfFailed(m_device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&commandAllocator)));
    return commandAllocator;
}

ComPtr<ID3D12GraphicsCommandList7> CommandQueue::CreateCommandList(ID3D12CommandAllocator* pCommandAllocator)
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12GraphicsCommandList7> commandList7;
    ThrowIfFailed(m_device->CreateCommandList(0, m_type, pCommandAllocator, nullptr, IID_PPV_ARGS(&commandList)));
    ThrowIfFailed(commandList.As(&commandList7));
    return commandList7;
}

std::pair<ComPtr<ID3D12CommandAllocator>, CommandList> CommandQueue::GetAvailableCommandList()
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList7> commandList;

    // Command allocator queue에 GPU 작업이 끝난 allocator가 존재한다면 그것을 사용하고 없다면 새로 생성
    if (!m_commandAllocatorQueue.empty() && IsFenceComplete(m_commandAllocatorQueue.front().fenceValue))
    {
        commandAllocator = m_commandAllocatorQueue.front().commandAllocator;
        m_commandAllocatorQueue.pop();
        ThrowIfFailed(commandAllocator->Reset());
    }
    else
    {
        commandAllocator = CreateCommandAllocator();
    }

    // Command list는 execute만 되면 즉시 재사용이 가능하므로 큐에 있으면 바로 사용하고 없으면 새로 생성
    // 직전에 얻은 commandAllocator에 연결
    if (!m_commandListQueue.empty())
    {
        commandList = m_commandListQueue.front();
        m_commandListQueue.pop();
        ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
    }
    else
    {
        commandList = CreateCommandList(commandAllocator.Get());
    }

    // Bind with DescriptorHeap
    ID3D12DescriptorHeap* ppHeaps[] = { m_dynamicDescriptorHeap.GetCurrentDescriptorHeap().Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // 커맨드 리스트는 반환 시 래퍼로 감싼다.
    // CommandList 클래스에서 제공하는 기능들은 순수하게 커맨드 리스트가 작성될 때에만 필요하므로
    // 풀 내에 있을 때에는 래퍼 클래스의 기능이 필요없다.
    // piecewise_construct를 사용하여 in-place construction을 수행.
    return std::pair<ComPtr<ID3D12CommandAllocator>, CommandList>(
        std::piecewise_construct,
        std::forward_as_tuple(commandAllocator),
        std::forward_as_tuple(m_device, commandList));
}

UINT64 CommandQueue::ExecuteCommandLists(const ComPtr<ID3D12CommandAllocator>& commandAllocator, const CommandList& commandList, ResourceLayoutTracker& layoutTracker)
{
    // Get pending barriers and prepare the sub command list for sync
    auto [subCommandAllocator, subCommandList] = GetAvailableCommandList();
    ComPtr<ID3D12GraphicsCommandList7> sub = subCommandList.GetCommandList();

    auto pendingBarriers = commandList.GetPendingBarriers();

    if (!pendingBarriers.empty())
    {
        for (auto& barrier : pendingBarriers)
        {
            UINT subresourceIndex = barrier.Subresources.IndexOrFirstMipLevel;
            barrier.LayoutBefore = layoutTracker.GetLayout(barrier.pResource, subresourceIndex);
        }

        D3D12_BARRIER_GROUP barrierGroups[] = { TextureBarrierGroup(UINT32(pendingBarriers.size()), pendingBarriers.data()) };
        sub->Barrier(1, barrierGroups);
        sub->Close();
    }
    else
    {
        sub->Close();
    }

    // Prepare main command list
    auto main = commandList.GetCommandList();
    main->Close();

    // Execute command lists
    if (!pendingBarriers.empty())
    {
        ID3D12CommandList* ppCommandLists[] = { sub.Get(), main.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    }
    else
    {
        ID3D12CommandList* ppCommandLists[] = { main.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    }

    // Update ResourceLayoutTracker
    auto latestLayouts = commandList.GetLatestLayouts();
    for (const auto& [pResource, p] : latestLayouts)
    {
        const auto& [layoutInfo, isNotUsed] = p;
        UINT subresourceCount = layoutInfo.MipLevels * layoutInfo.DepthOrArraySize * layoutInfo.PlaneCount;
        for (UINT i = 0; i < subresourceCount; i++)
        {
            if (isNotUsed[i]) continue;     // 커맨드 리스트 내에서 사용한 적이 없는 서브리소스는 그대로 유지
            layoutTracker.SetLayout(pResource, i, layoutInfo.GetLayout(i));
        }
    }

    // Set sub and main with same fenceValue for now
    UINT64 fenceValue = Signal();
    m_commandAllocatorQueue.push({ fenceValue, commandAllocator });
    m_commandListQueue.push(main);
    m_commandAllocatorQueue.push({ fenceValue, subCommandAllocator });
    m_commandListQueue.push(sub);

    return fenceValue;
}

UINT64 CommandQueue::Signal()
{
    m_fenceValue++;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));
    return m_fenceValue;
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