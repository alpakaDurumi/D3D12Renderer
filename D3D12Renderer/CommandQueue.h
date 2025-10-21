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
            commandList = CreateCommandList(commandAllocator);
        }

        return { commandAllocator, commandList };
    }

    UINT64 ExecuteCommandLists(ComPtr<ID3D12CommandAllocator>& commandAllocator, ComPtr<ID3D12GraphicsCommandList7>& commandList)
    {
        commandList->Close();

        ID3D12CommandList* ppCommandLists[] = { commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

        // Signal로 얻은 fenceValue를 사용? -> 각 command allocator가 작업이 끝났는지 알기 위한 수단
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

    // 각 command allocator의 GPU 사용이 모두 끝났는지를 확인하기 위해 fence value가 짝지어져 있음
    struct CommandAllocatorEntry
    {
        UINT64 fenceValue;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    // 현재 GPU에서 사용중인 command allocator와 command list의 목록을 담고 있는 std::queue들
    std::queue<CommandAllocatorEntry> m_commandAllocatorQueue;
    std::queue<ComPtr<ID3D12GraphicsCommandList7>> m_commandListQueue;

    ComPtr<ID3D12CommandQueue> m_commandQueue;

    ComPtr<ID3D12Device> m_device;

    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;
};