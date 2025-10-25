#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "D3DHelper.h"

using namespace D3DHelper;

struct ResourceLayoutInfo
{
    ResourceLayoutInfo(UINT mipLevels, UINT depthOrArraySize, UINT8 planeCount, D3D12_BARRIER_LAYOUT initialLayout)
        : MipLevels(mipLevels), DepthOrArraySize(depthOrArraySize), PlaneCount(planeCount), Layouts(mipLevels* depthOrArraySize* planeCount, initialLayout)
    {
    }

    D3D12_BARRIER_LAYOUT GetLayout(UINT subresourceIndex)
    {
        assert(subresourceIndex < Layouts.size());
        return Layouts[subresourceIndex];
    }

    void SetLayout(UINT subresourceIndex, D3D12_BARRIER_LAYOUT layout)
    {
        assert(subresourceIndex < Layouts.size());
        Layouts[subresourceIndex] = layout;
    }

    UINT MipLevels;
    UINT DepthOrArraySize;
    UINT8 PlaneCount;
    std::vector<D3D12_BARRIER_LAYOUT> Layouts;
};

// Track layout of resource which type is texture
// 각 서브리소스의 레이아웃을 기억해야 한다
class ResourceLayoutTracker
{
public:
    ResourceLayoutTracker()
    {
    }

    void RegisterResource(ID3D12Device* pDevice, ID3D12Resource* pResource, D3D12_BARRIER_LAYOUT initialLayout, UINT depthOrArraySize, UINT mipLevels, DXGI_FORMAT format)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        UINT8 planeCount = GetFormatPlaneCount(pDevice, format);
        ResourceLayoutInfo newInfo(mipLevels, depthOrArraySize, planeCount, initialLayout);
        m_resourceLayoutMap.insert({ pResource, newInfo });
    }

    void UnregisterResource(ID3D12Resource* pResource)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (m_resourceLayoutMap.find(pResource) != m_resourceLayoutMap.end())
        {
            m_resourceLayoutMap.erase(pResource);
        }
    }

    std::pair<D3D12_BARRIER_LAYOUT, UINT> SetLayout(ID3D12Resource* pResource, UINT mipIndex, UINT arrayIndex, UINT planeIndex, D3D12_BARRIER_LAYOUT layout)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_resourceLayoutMap.find(pResource);
        assert(it != m_resourceLayoutMap.end());

        ResourceLayoutInfo& layoutInfo = it->second;
        UINT subresourceIndex = CalcSubresourceIndex(mipIndex, arrayIndex, planeIndex, layoutInfo.MipLevels, layoutInfo.DepthOrArraySize);
        D3D12_BARRIER_LAYOUT layoutBefore = layoutInfo.GetLayout(subresourceIndex);
        layoutInfo.SetLayout(subresourceIndex, layout);

        return { layoutBefore, subresourceIndex };
    }

    D3D12_BARRIER_LAYOUT SetLayout(ID3D12Resource* pResource, UINT subresourceIndex, D3D12_BARRIER_LAYOUT layout)
    {
        std::lock_guard<std::mutex> lock(m_mutex);

        auto it = m_resourceLayoutMap.find(pResource);
        assert(it != m_resourceLayoutMap.end());

        ResourceLayoutInfo& layoutInfo = it->second;
        D3D12_BARRIER_LAYOUT layoutBefore = layoutInfo.GetLayout(subresourceIndex);
        layoutInfo.SetLayout(subresourceIndex, layout);

        return layoutBefore;
    }

private:
    std::unordered_map<ID3D12Resource*, ResourceLayoutInfo> m_resourceLayoutMap;
    std::mutex m_mutex;
};