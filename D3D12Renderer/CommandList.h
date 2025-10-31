#pragma once

#include <wrl/client.h>
#include "d3d12.h"
#include <unordered_map>
#include <vector>
#include "ResourceLayoutTracker.h"

using Microsoft::WRL::ComPtr;

// Wrapper for Command List. Only accessed by a single thread at a time.
class CommandList
{
public:
    // Disable copy and move. Only use as l-value reference
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;
    CommandList(CommandList&&) = delete;
    CommandList& operator=(CommandList&&) = delete;

    CommandList(ComPtr<ID3D12GraphicsCommandList7>& commandList, ComPtr<ID3D12Device10>& device)
        : m_commandList(commandList), m_device(device)
    {
    }

    ComPtr<ID3D12GraphicsCommandList7> GetCommandList() const { return m_commandList; }

    void Barrier(
        ID3D12Resource* pResource,
        D3D12_BARRIER_SYNC syncBefore,
        D3D12_BARRIER_SYNC syncAfter,
        D3D12_BARRIER_ACCESS accessBefore,
        D3D12_BARRIER_ACCESS accessAfter);

    void Barrier(
        ID3D12Resource* pResource,
        ResourceLayoutTracker& layoutTracker,
        D3D12_BARRIER_SYNC syncBefore,
        D3D12_BARRIER_SYNC syncAfter,
        D3D12_BARRIER_ACCESS accessBefore,
        D3D12_BARRIER_ACCESS accessAfter,
        D3D12_BARRIER_LAYOUT layoutAfter,
        D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange);

    std::vector<D3D12_TEXTURE_BARRIER> GetPendingBarriers() const { return m_pendingBarriers; }

    std::unordered_map<ID3D12Resource*, std::pair<ResourceLayoutInfo, std::vector<bool>>> GetLatestLayouts() const { return m_latestLayouts; }

private:
    ComPtr<ID3D12Device10> m_device;

    ComPtr<ID3D12GraphicsCommandList7> m_commandList;
    std::unordered_map<ID3D12Resource*, std::pair<ResourceLayoutInfo, std::vector<bool>>> m_latestLayouts;  // 이번 command list 작성 중, 각 리소스에 대해 알려진 최신 layout
    std::vector<D3D12_TEXTURE_BARRIER> m_pendingBarriers;
};