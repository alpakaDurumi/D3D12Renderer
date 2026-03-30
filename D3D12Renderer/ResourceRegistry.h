#pragma once

#include <Windows.h>

#include <d3d12.h>

#include <vector>

#include "D3DHelper.h"
#include "Aliases.h"

class ResourceRegistry
{
public:
    ResourceHandle RegisterStatic(ID3D12Device* pDevice, ID3D12Resource* pResource)
    {
        auto desc = pResource->GetDesc();

        Entry newEntry;
        newEntry.pResources.push_back(pResource);
        newEntry.mipLevels = desc.MipLevels;
        newEntry.depthOrArraySize = desc.DepthOrArraySize;
        newEntry.planeCount = D3DHelper::GetFormatPlaneCount(pDevice, desc.Format);
        newEntry.isTexture3D = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        m_entries.push_back(newEntry);
        return static_cast<ResourceHandle>(m_entries.size()) - 1;
    }

    ResourceHandle RegisterPerFrame(ID3D12Device* pDevice, std::vector<ID3D12Resource*>& pResources)
    {
        auto desc = pResources[0]->GetDesc();

        Entry newEntry;
        newEntry.pResources = pResources;
        newEntry.mipLevels = desc.MipLevels;
        newEntry.depthOrArraySize = desc.DepthOrArraySize;
        newEntry.planeCount = D3DHelper::GetFormatPlaneCount(pDevice, desc.Format);
        newEntry.isTexture3D = desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        m_entries.push_back(newEntry);
        return static_cast<ResourceHandle>(m_entries.size()) - 1;
    }

    void UpdateStatic(ResourceHandle handle, ID3D12Resource* pResource)
    {
        m_entries[handle].pResources[0] = pResource;
    }

    void UpdatePerFrame(ResourceHandle handle, std::vector<ID3D12Resource*> pResources)
    {
        m_entries[handle].pResources = pResources;
    }

    ID3D12Resource* Resolve(ResourceHandle handle, UINT frameIndex) const
    {
        bool isStatic = m_entries[handle].pResources.size() == 1;
        return isStatic ? m_entries[handle].pResources[0] : m_entries[handle].pResources[frameIndex];
    }

    UINT GetSubresourceCount(ResourceHandle handle) const
    {
        Entry e = m_entries[handle];

        UINT ret = e.mipLevels * e.planeCount;
        if (!e.isTexture3D) ret *= e.depthOrArraySize;

        return ret;
    }

    std::tuple<UINT, UINT, UINT> GetResourceDimension(ResourceHandle handle) const
    {
        Entry e = m_entries[handle];
        return std::make_tuple(e.mipLevels, e.depthOrArraySize, e.planeCount);
    }

    UINT GetEntryCount() const
    {
        return static_cast<UINT>(m_entries.size());
    }

private:
    struct Entry
    {
        std::vector<ID3D12Resource*> pResources;
        UINT mipLevels;
        UINT depthOrArraySize;
        UINT planeCount;
        bool isTexture3D;
    };

    std::vector<Entry> m_entries;
};