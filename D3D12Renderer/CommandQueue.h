#pragma once

#include <wrl/client.h>

#include <d3d12.h>

#include <queue>
#include <utility>  // for std::pair

#include "CommandList.h"

class ResourceLayoutTracker;

using Microsoft::WRL::ComPtr;

class CommandQueue
{
public:
    // Disable copy and move. Only use as l-value reference
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;
    CommandQueue(CommandQueue&&) = delete;
    CommandQueue& operator=(CommandQueue&&) = delete;

    CommandQueue(ComPtr<ID3D12Device10>& device, D3D12_COMMAND_LIST_TYPE type);

    ~CommandQueue();

    ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return m_commandQueue; }

    ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    ComPtr<ID3D12GraphicsCommandList7> CreateCommandList(ComPtr<ID3D12CommandAllocator>& commandAllocator);

    std::pair<ComPtr<ID3D12CommandAllocator>, CommandList> GetAvailableCommandList();

    UINT64 ExecuteCommandLists(ComPtr<ID3D12CommandAllocator>& commandAllocator, CommandList& commandList, ResourceLayoutTracker& layoutTracker);

    UINT64 Signal();
    bool IsFenceComplete(UINT64 fenceValue);
    void WaitForFenceValue(UINT64 fenceValue);
    void Flush();

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

    ComPtr<ID3D12Device10> m_device;

    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;
};