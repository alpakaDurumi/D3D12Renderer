#pragma once

#include <Windows.h>

#include <d3d12.h>

#include <vector>
#include <cassert>

#include "D3DHelper.h"
#include "Aliases.h"

class ResourceRegistry
{
public:
    ResourceHandle Register(bool isPerFrame)
    {
        ResourceHandle ret = static_cast<ResourceHandle>(m_entries.size());

        Entry newEntry;
        newEntry.elementCount = 0;
        newEntry.isPerFrame = isPerFrame;
        m_entries.push_back(newEntry);

        return ret;
    }

    void AddElement(ResourceHandle handle, std::vector<ID3D12Resource*> pResources)
    {
        auto& entry = m_entries[handle];
        assert((entry.isPerFrame && pResources.size() == m_frameCount) || (!entry.isPerFrame && pResources.size() == 1));
        entry.pResources.insert(entry.pResources.end(), pResources.begin(), pResources.end());
        ++entry.elementCount;
    }

    void UpdateElement(ResourceHandle handle, UINT elementIndex, std::vector<ID3D12Resource*> pResources)
    {
        auto& entry = m_entries[handle];
        assert((entry.isPerFrame && pResources.size() == m_frameCount) || (!entry.isPerFrame && pResources.size() == 1));
        UINT offset = entry.isPerFrame ? elementIndex * m_frameCount : elementIndex;
        std::copy(pResources.begin(), pResources.end(), entry.pResources.begin() + offset);
    }

    ID3D12Resource* Resolve(ResourceHandle handle, UINT elementIndex, UINT frameIndex) const
    {
        assert(elementIndex < m_entries[handle].elementCount && frameIndex < m_frameCount);

        bool isPerFrame = m_entries[handle].isPerFrame;
        UINT idx = isPerFrame ? elementIndex * m_frameCount + frameIndex : elementIndex;

        return m_entries[handle].pResources[idx];
    }

    UINT GetSubresourceCount(ID3D12Device* pDevice, ResourceHandle handle) const
    {
        assert(handle < m_entries.size() && !m_entries[handle].pResources.empty());

        auto desc = m_entries[handle].pResources.front()->GetDesc();
        UINT ret = desc.MipLevels * GetFormatPlaneCount(pDevice, desc.Format);
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D) ret *= desc.DepthOrArraySize;

        return ret;
    }

    std::tuple<UINT, UINT, UINT> GetResourceDimension(ID3D12Device* pDevice, ResourceHandle handle) const
    {
        assert(handle < m_entries.size() && !m_entries[handle].pResources.empty());

        auto desc = m_entries[handle].pResources.front()->GetDesc();
        UINT8 planeCount = GetFormatPlaneCount(pDevice, desc.Format);

        return { desc.MipLevels, desc.DepthOrArraySize, planeCount };
    }

    UINT GetEntryCount() const
    {
        return static_cast<UINT>(m_entries.size());
    }

    void SetFrameCount(UINT frameCount)
    {
        m_frameCount = frameCount;
    }

    UINT GetElementCount(ResourceHandle handle) const
    {
        return m_entries[handle].elementCount;
    }

private:
    struct Entry
    {
        std::vector<ID3D12Resource*> pResources;    // size = elementCount * (isPerFrame ? frameCount : 1)
        UINT elementCount;
        bool isPerFrame;
    };

    std::vector<Entry> m_entries;
    UINT m_frameCount;
};