#pragma once

#include <wrl/client.h>

#include "d3d12.h"

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

    CommandList(const ComPtr<ID3D12Device10>& device, const ComPtr<ID3D12GraphicsCommandList7>& commandList)
        : m_device(device), m_commandList(commandList)
    {
    }

    ComPtr<ID3D12GraphicsCommandList7> GetCommandList() const { return m_commandList; }

    void Barrier(
        ID3D12Resource* pResource,
        D3D12_BARRIER_SYNC syncBefore,
        D3D12_BARRIER_SYNC syncAfter,
        D3D12_BARRIER_ACCESS accessBefore,
        D3D12_BARRIER_ACCESS accessAfter);

private:
    ComPtr<ID3D12Device10> m_device;
    ComPtr<ID3D12GraphicsCommandList7> m_commandList;
};