#include "CommandQueue.h"

#include "ResourceLayoutTracker.h"
#include "D3DHelper.h"

using namespace D3DHelper;

CommandQueue::CommandQueue(ComPtr<ID3D12Device10>& device, D3D12_COMMAND_LIST_TYPE type)
    : m_type(type), m_fenceValue(0), m_device(device)
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

ComPtr<ID3D12GraphicsCommandList7> CommandQueue::CreateCommandList(ComPtr<ID3D12CommandAllocator>& commandAllocator)
{
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12GraphicsCommandList7> commandList7;
    ThrowIfFailed(m_device->CreateCommandList(0, m_type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
    ThrowIfFailed(commandList.As(&commandList7));
    return commandList7;
}

std::pair<ComPtr<ID3D12CommandAllocator>, CommandList> CommandQueue::GetAvailableCommandList()
{
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList7> commandList;

    // Command allocator queue�� GPU �۾��� ���� allocator�� �����Ѵٸ� �װ��� ����ϰ� ���ٸ� ���� ����
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

    // Command list�� execute�� �Ǹ� ��� ������ �����ϹǷ� ť�� ������ �ٷ� ����ϰ� ������ ���� ����
    // ������ ���� commandAllocator�� ����
    if (!m_commandListQueue.empty())
    {
        commandList = m_commandListQueue.front();
        m_commandListQueue.pop();
        ThrowIfFailed(commandList->Reset(commandAllocator.Get(), nullptr));
    }
    else
    {
        commandList = CreateCommandList(commandAllocator);
    }

    // Ŀ�ǵ� ����Ʈ�� ��ȯ �� ���۷� ���Ѵ�.
    // CommandList Ŭ�������� �����ϴ� ��ɵ��� �����ϰ� Ŀ�ǵ� ����Ʈ�� �ۼ��� ������ �ʿ��ϹǷ�
    // Ǯ ���� ���� ������ ���� Ŭ������ ����� �ʿ����.
    // piecewise_construct�� ����Ͽ� in-place construction�� ����.
    return std::pair<ComPtr<ID3D12CommandAllocator>, CommandList>(
        std::piecewise_construct,
        std::forward_as_tuple(commandAllocator),
        std::forward_as_tuple(commandList, m_device));
}

UINT64 CommandQueue::ExecuteCommandLists(ComPtr<ID3D12CommandAllocator>& commandAllocator, CommandList& commandList, ResourceLayoutTracker& layoutTracker)
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

    // layoutTracker ������Ʈ
    auto latestLayouts = commandList.GetLatestLayouts();
    for (const auto& [pResource, p] : latestLayouts)
    {
        const auto& [layoutInfo, isNotUsed] = p;
        UINT subresourceCount = layoutInfo.MipLevels * layoutInfo.DepthOrArraySize * layoutInfo.PlaneCount;
        for (UINT i = 0; i < subresourceCount; i++)
        {
            if (isNotUsed[i]) continue;     // Ŀ�ǵ� ����Ʈ ������ ����� ���� ���� ���긮�ҽ��� �״�� ����
            layoutTracker.SetLayout(pResource, i, layoutInfo.GetLayout(i));
        }
    }

    // Signal�� ���� fenceValue�� ���? -> �� command allocator�� �۾��� �������� �˱� ���� ����
    // �ϴ��� �� �� ������ fenceValue�� ǥ��
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

bool CommandQueue::IsFenceComplete(UINT64 fenceValue)
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