#pragma once

#include <queue>
#include <utility> // for std::pair
#include <vector>

#include <basetsd.h>
#include <d3d12.h>
#include <wrl/client.h>

class DynamicDescriptorHeap;

class CommandQueue
{
public:
    // Disable copy and move. Only use as l-value reference
    CommandQueue(const CommandQueue&) = delete;
    CommandQueue& operator=(const CommandQueue&) = delete;
    CommandQueue(CommandQueue&&) = delete;
    CommandQueue& operator=(CommandQueue&&) = delete;

    CommandQueue() = default;
    ~CommandQueue();

    void Init(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE type);

    void SetDescriptorHeaps(const DynamicDescriptorHeap* pHeapForCbvSrvUav, ID3D12DescriptorHeap* pHeapForSampler);

    ID3D12CommandQueue* GetCommandQueue() const;

    std::pair<ID3D12CommandAllocator*, ID3D12GraphicsCommandList7*> GetAvailableCommandList();

    UINT64 ExecuteCommandLists(ID3D12CommandAllocator* pCommandAllocator, ID3D12GraphicsCommandList7* pCommandList);

    UINT64 Signal();
    UINT64 GetCompletedFenceValue() const;
    bool IsFenceComplete(UINT64 fenceValue) const;
    void WaitForFenceValue(UINT64 fenceValue);
    void Flush();

private:
    ID3D12CommandAllocator* CreateCommandAllocator();
    ID3D12GraphicsCommandList7* CreateCommandList(ID3D12CommandAllocator* pCommandAllocator);

    D3D12_COMMAND_LIST_TYPE m_type;

    struct CommandAllocatorEntry
    {
        UINT64 fenceValue;
        ID3D12CommandAllocator* pCommandAllocator;
    };

    // Pools
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_commandAllocatorPool;
    std::vector<Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList7>> m_commandListPool;

    // Queue containing command allocators and lists currently being used by GPU
    std::queue<CommandAllocatorEntry> m_commandAllocatorQueue;
    std::queue<ID3D12GraphicsCommandList7*> m_commandListQueue;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    HANDLE m_fenceEvent = nullptr;
    UINT64 m_fenceValue = 0;

    ID3D12Device* m_pDevice = nullptr;
    const DynamicDescriptorHeap* m_pDynamicDescriptorHeapForCbvSrvUav = nullptr;
    ID3D12DescriptorHeap* m_pSamplerDescriptorHeap = nullptr;
};
