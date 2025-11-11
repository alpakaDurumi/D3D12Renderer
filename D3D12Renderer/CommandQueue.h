#pragma once

#include <wrl/client.h>

#include <d3d12.h>

#include <queue>
#include <utility>  // for std::pair

#include "CommandList.h"

class ResourceLayoutTracker;
class DynamicDescriptorHeap;

using Microsoft::WRL::ComPtr;

class CommandQueue
{
public:
    // Disable copy and move. Only use as l-value reference
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;
    CommandQueue(CommandQueue&&) = delete;
    CommandQueue& operator=(CommandQueue&&) = delete;

    CommandQueue(const ComPtr<ID3D12Device10>& device, DynamicDescriptorHeap& dynamicDescriptorHeap, D3D12_COMMAND_LIST_TYPE type);

    ~CommandQueue();

    ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return m_commandQueue; }

    ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    ComPtr<ID3D12GraphicsCommandList7> CreateCommandList(ID3D12CommandAllocator* pCommandAllocator);

    std::pair<ComPtr<ID3D12CommandAllocator>, CommandList> GetAvailableCommandList();

    UINT64 ExecuteCommandLists(const ComPtr<ID3D12CommandAllocator>& commandAllocator, const CommandList& commandList, ResourceLayoutTracker& layoutTracker);

    UINT64 Signal();
    bool IsFenceComplete(UINT64 fenceValue) const;
    void WaitForFenceValue(UINT64 fenceValue);
    void Flush();

private:
    D3D12_COMMAND_LIST_TYPE m_type;

    struct CommandAllocatorEntry
    {
        UINT64 fenceValue;
        ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    // Queue containing command allocators and lists currently being used by GPU
    std::queue<CommandAllocatorEntry> m_commandAllocatorQueue;
    std::queue<ComPtr<ID3D12GraphicsCommandList7>> m_commandListQueue;

    ComPtr<ID3D12CommandQueue> m_commandQueue;

    ComPtr<ID3D12Device10> m_device;

    ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent;
    UINT64 m_fenceValue;

    DynamicDescriptorHeap& m_dynamicDescriptorHeap;
};