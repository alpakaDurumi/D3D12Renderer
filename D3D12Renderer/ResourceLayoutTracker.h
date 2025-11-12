#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include <mutex>
#include <unordered_map>
#include <vector>
#include <utility>

using Microsoft::WRL::ComPtr;

struct ResourceLayoutInfo
{
    ResourceLayoutInfo(UINT mipLevels, UINT depthOrArraySize, UINT8 planeCount, D3D12_BARRIER_LAYOUT initialLayout)
        : MipLevels(mipLevels), DepthOrArraySize(depthOrArraySize), PlaneCount(planeCount), Layouts(mipLevels* depthOrArraySize* planeCount, initialLayout)
    {
    }

    D3D12_BARRIER_LAYOUT GetLayout(UINT subresourceIndex) const;

    void SetLayout(UINT subresourceIndex, D3D12_BARRIER_LAYOUT layout);

    UINT MipLevels;
    UINT DepthOrArraySize;
    UINT8 PlaneCount;
    std::vector<D3D12_BARRIER_LAYOUT> Layouts;
};

// Track layout of each subresources which type is texture
class ResourceLayoutTracker
{
public:
    // Disable copy and move. Only use as l-value reference
    ResourceLayoutTracker(const ResourceLayoutTracker&) = delete;
    ResourceLayoutTracker& operator=(const ResourceLayoutTracker&) = delete;
    ResourceLayoutTracker(ResourceLayoutTracker&&) = delete;
    ResourceLayoutTracker& operator=(ResourceLayoutTracker&&) = delete;

    ResourceLayoutTracker(const ComPtr<ID3D12Device10>& device)
        : m_device(device)
    {
    }

    void RegisterResource(ID3D12Resource* pResource, D3D12_BARRIER_LAYOUT initialLayout, UINT depthOrArraySize, UINT mipLevels, DXGI_FORMAT format);
    void UnregisterResource(ID3D12Resource* pResource);

    D3D12_BARRIER_LAYOUT GetLayout(ID3D12Resource* pResource, UINT subresourceIndex);

    std::pair<D3D12_BARRIER_LAYOUT, UINT> SetLayout(ID3D12Resource* pResource, UINT mipIndex, UINT arrayIndex, UINT planeIndex, D3D12_BARRIER_LAYOUT layout);
    D3D12_BARRIER_LAYOUT SetLayout(ID3D12Resource* pResource, UINT subresourceIndex, D3D12_BARRIER_LAYOUT layout);

private:
    std::unordered_map<ID3D12Resource*, ResourceLayoutInfo> m_resourceLayoutMap;
    std::mutex m_mutex;
    ComPtr<ID3D12Device10> m_device;
};