#include "pch.h"
#include "ResourceLayoutTracker.h"

#include "D3DHelper.h"

using namespace D3DHelper;

void ResourceLayoutTracker::RegisterResource(ID3D12Resource* pResource, D3D12_BARRIER_LAYOUT initialLayout, UINT depthOrArraySize, UINT mipLevels, DXGI_FORMAT format)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    UINT8 planeCount = GetFormatPlaneCount(m_device.Get(), format);
    ResourceLayoutInfo newInfo(mipLevels, depthOrArraySize, planeCount, initialLayout);
    m_resourceLayoutMap.insert({ pResource, newInfo });
}

void ResourceLayoutTracker::UnregisterResource(ID3D12Resource* pResource)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_resourceLayoutMap.find(pResource) != m_resourceLayoutMap.end())
    {
        m_resourceLayoutMap.erase(pResource);
    }
}

D3D12_BARRIER_LAYOUT ResourceLayoutTracker::GetLayout(ID3D12Resource* pResource, UINT subresourceIndex)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceLayoutMap.find(pResource);
    assert(it != m_resourceLayoutMap.end());

    return it->second.GetLayout(subresourceIndex);
}

std::pair<D3D12_BARRIER_LAYOUT, UINT> ResourceLayoutTracker::SetLayout(ID3D12Resource* pResource, UINT mipIndex, UINT arrayIndex, UINT planeIndex, D3D12_BARRIER_LAYOUT layout)
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

D3D12_BARRIER_LAYOUT ResourceLayoutTracker::SetLayout(ID3D12Resource* pResource, UINT subresourceIndex, D3D12_BARRIER_LAYOUT layout)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_resourceLayoutMap.find(pResource);
    assert(it != m_resourceLayoutMap.end());

    ResourceLayoutInfo& layoutInfo = it->second;
    D3D12_BARRIER_LAYOUT layoutBefore = layoutInfo.GetLayout(subresourceIndex);
    layoutInfo.SetLayout(subresourceIndex, layout);

    return layoutBefore;
}

D3D12_BARRIER_LAYOUT ResourceLayoutInfo::GetLayout(UINT subresourceIndex) const
{
    assert(subresourceIndex < Layouts.size());
    return Layouts[subresourceIndex];
}

void ResourceLayoutInfo::SetLayout(UINT subresourceIndex, D3D12_BARRIER_LAYOUT layout)
{
    assert(subresourceIndex < Layouts.size());
    Layouts[subresourceIndex] = layout;
}