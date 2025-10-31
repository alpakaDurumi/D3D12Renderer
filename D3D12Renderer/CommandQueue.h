#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <queue>
#include "D3DHelper.h"
#include "CommandList.h"
#include "ResourceLayoutTracker.h"

#include <utility>  // for std::pair

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

class CommandQueue
{
public:
    // Disable copy and move. Only use as l-value reference
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;
    CommandQueue(CommandQueue&&) = delete;
    CommandQueue& operator=(CommandQueue&&) = delete;

    CommandQueue(ComPtr<ID3D12Device10>& device, D3D12_COMMAND_LIST_TYPE type)
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

    ~CommandQueue()
    {
        CloseHandle(m_fenceEvent);
    }

    ComPtr<ID3D12CommandQueue> GetCommandQueue() const
    {
        return m_commandQueue;
    }

    ComPtr<ID3D12CommandAllocator> CreateCommandAllocator()
    {
        ComPtr<ID3D12CommandAllocator> commandAllocator;
        ThrowIfFailed(m_device->CreateCommandAllocator(m_type, IID_PPV_ARGS(&commandAllocator)));
        return commandAllocator;
    }

    ComPtr<ID3D12GraphicsCommandList7> CreateCommandList(ComPtr<ID3D12CommandAllocator>& commandAllocator)
    {
        ComPtr<ID3D12GraphicsCommandList> commandList;
        ComPtr<ID3D12GraphicsCommandList7> commandList7;
        ThrowIfFailed(m_device->CreateCommandList(0, m_type, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
        ThrowIfFailed(commandList.As(&commandList7));
        return commandList7;
    }

    std::pair<ComPtr<ID3D12CommandAllocator>, CommandList> GetCommandList()
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

    //UINT64 ExecuteCommandLists(ComPtr<ID3D12CommandAllocator>& commandAllocator, ComPtr<ID3D12GraphicsCommandList7>& commandList)
    //{
    //    commandList->Close();

    //    ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
    //    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    //    // Signal�� ���� fenceValue�� ���? -> �� command allocator�� �۾��� �������� �˱� ���� ����
    //    uint64_t fenceValue = Signal();
    //    m_commandAllocatorQueue.push({ fenceValue, commandAllocator });
    //    m_commandListQueue.push(commandList);

    //    return fenceValue;
    //}

    UINT64 ExecuteCommandLists(ComPtr<ID3D12CommandAllocator>& commandAllocator, CommandList& commandList, ResourceLayoutTracker& layoutTracker)
    {
        // Get pending barriers and prepare the sub command list for sync
        auto [subCommandAllocator, subCommandList] = GetCommandList();
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

    UINT64 Signal()
    {
        m_fenceValue++;
        ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_fenceValue));
        return m_fenceValue;
    }

    bool IsFenceComplete(UINT64 fenceValue)
    {
        return m_fence->GetCompletedValue() >= fenceValue;
    }

    void WaitForFenceValue(UINT64 fenceValue)
    {
        if (!IsFenceComplete(fenceValue))
        {
            ThrowIfFailed(m_fence->SetEventOnCompletion(fenceValue, m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
        }
    }

    // Wait for pending GPU work to complete
    void Flush()
    {
        UINT64 fenceValue = Signal();
        WaitForFenceValue(fenceValue);
    }

private:
    D3D12_COMMAND_LIST_TYPE m_type;

    // �� command allocator�� GPU ����� ��� ���������� Ȯ���ϱ� ���� fence value�� ¦������ ����
    struct CommandAllocatorEntry
    {
        UINT64 fenceValue;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    // ���� GPU���� ������� command allocator�� command list�� ����� ��� �ִ� std::queue��
    std::queue<CommandAllocatorEntry> m_commandAllocatorQueue;
    std::queue<ComPtr<ID3D12GraphicsCommandList7>> m_commandListQueue;

    ComPtr<ID3D12CommandQueue> m_commandQueue;

    ComPtr<ID3D12Device10> m_device;

    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;
};