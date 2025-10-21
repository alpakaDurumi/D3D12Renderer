#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <queue>
#include "D3DHelper.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

class CommandQueue
{
public:
    CommandQueue(ComPtr<ID3D12Device>& device, D3D12_COMMAND_LIST_TYPE type)
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

    std::pair<ComPtr<ID3D12CommandAllocator>, ComPtr<ID3D12GraphicsCommandList7>> GetCommandList()
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

        return { commandAllocator, commandList };
    }

    UINT64 ExecuteCommandLists(ComPtr<ID3D12CommandAllocator>& commandAllocator, ComPtr<ID3D12GraphicsCommandList7>& commandList)
    {
        commandList->Close();

        ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // Signal�� ���� fenceValue�� ���? -> �� command allocator�� �۾��� �������� �˱� ���� ����
        uint64_t fenceValue = Signal();
        m_commandAllocatorQueue.push({ fenceValue, commandAllocator });
        m_commandListQueue.push(commandList);

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

    ComPtr<ID3D12Device> m_device;

    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;
};