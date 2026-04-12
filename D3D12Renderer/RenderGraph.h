#pragma once

#include <Windows.h>

#include <d3d12.h>

#include <vector>
#include <array>
#include <cassert>
#include <algorithm>
#include <tuple>
#include <unordered_map>

#include "RenderGraphNode.h"
#include "CacheKeys.h"
#include "D3DHelper.h"
#include "Aliases.h"

class RenderGraph
{
public:
    void Init(ID3D12Device10* pDevice, UINT frameCount)
    {
        m_pDevice = pDevice;
        SetFrameCount(frameCount);
    }

    BufferHandle RegisterBuffer(const std::string& name, bool isPerFrame)
    {
        return { RegisterHelper(name, isPerFrame, m_bufferGroups, m_bufferMap, {}) };
    }

    TextureHandle RegisterTexture(const std::string& name, bool isPerFrame, TextureResourceUsage initialUsage)
    {
        return { RegisterHelper(name, isPerFrame, m_textureGroups, m_textureMap, initialUsage) };
    }

    void AddElement(BufferHandle handle, std::vector<ID3D12Resource*> pResources)
    {
        AddElementHelper(handle.index, pResources, m_bufferGroups);
    }

    void AddElement(TextureHandle handle, std::vector<ID3D12Resource*> pResources)
    {
        AddElementHelper(handle.index, pResources, m_textureGroups);
    }

    void UpdateElement(BufferHandle handle, UINT elementIndex, std::vector<ID3D12Resource*> pResources)
    {
        UpdateElementHelper(handle.index, elementIndex, pResources, m_bufferGroups);
    }

    void UpdateElement(TextureHandle handle, UINT elementIndex, std::vector<ID3D12Resource*> pResources)
    {
        UpdateElementHelper(handle.index, elementIndex, pResources, m_textureGroups);
    }

    ID3D12Resource* Resolve(BufferHandle handle, UINT elementIndex, UINT frameIndex) const
    {
        return ResolveHelper(handle.index, elementIndex, frameIndex, m_bufferGroups);
    }

    ID3D12Resource* Resolve(TextureHandle handle, UINT elementIndex, UINT frameIndex) const
    {
        return ResolveHelper(handle.index, elementIndex, frameIndex, m_textureGroups);
    }

    std::tuple<UINT, UINT, UINT> GetResourceDimension(ID3D12Device* pDevice, TextureHandle handle) const
    {
        auto desc = m_textureGroups[handle.index].pResources.front()->GetDesc();
        UINT8 planeCount = D3DHelper::GetFormatPlaneCount(pDevice, desc.Format);
        return { desc.MipLevels, desc.DepthOrArraySize, planeCount };
    }

    //UINT GetEntryCount() const
    //{
    //    return static_cast<UINT>(m_entries.size());
    //}

    void SetFrameCount(UINT frameCount)
    {
        m_frameCount = frameCount;
    }

    UINT GetElementCount(BufferHandle handle) const
    {
        return m_bufferGroups[handle.index].elementCount;
    }

    UINT GetElementCount(TextureHandle handle) const
    {
        return m_textureGroups[handle.index].elementCount;
    }

    BufferHandle GetBufferHandle(const std::string& name)
    {
        return { m_bufferMap.at(name) };
    }

    TextureHandle GetTextureHandle(const std::string& name)
    {
        return { m_textureMap.at(name) };
    }

    void Compile()
    {
        std::vector<BufferResourceUsage> currentBufferUsages(m_bufferGroups.size());

        // Use initialLayout when using newly created resources.
        // For existing resources, use values queried from m_frameEndUsage.
        std::vector<std::vector<TextureResourceUsage>> currentTextureUsages(m_textureGroups.size());
        for (UINT i = 0; i < m_textureGroups.size(); ++i)
        {
            auto& group = m_textureGroups[i];
            UINT subresourceCount = D3DHelper::GetSubresourceCount(m_pDevice, group.pResources.front());
            currentTextureUsages[i] = std::vector<TextureResourceUsage>(subresourceCount, group.initialUsage);
        }

        PassType defaultOrder[static_cast<std::size_t>(PassType::NUM_PASS_TYPES)] = {
            PassType::DEPTH_ONLY,
            PassType::GBUFFER,
            PassType::DEFERRED_LIGHTING,
            PassType::FORWARD_COLORING };

        // Compile graph
        for (const PassType& passType : defaultOrder)
        {
            auto& node = m_nodes[static_cast<UINT>(passType)];

            // Process buffer inputs
            for (auto& [handle, usage] : node.bufferInputs)
            {
                CompiledBufferBarrier barrier = { handle, currentBufferUsages[handle.index], usage };
                node.bufferBarriers.push_back(barrier);
            }

            // Process texture inputs
            for (auto& [handle, usage, range] : node.textureInputs)
            {
                auto& latestUsages = currentTextureUsages[handle.index];

                const auto& [IndexOrFirstMipLevel, NumMipLevels, FirstArraySlice, NumArraySlices, FirstPlane, NumPlanes] = range;

                if (IndexOrFirstMipLevel == 0xffff'ffff && NumMipLevels == 0)
                {
                    UINT subresourceCount = D3DHelper::GetSubresourceCount(m_pDevice, m_textureGroups[handle.index].pResources.front());

                    for (UINT i = 0; i < subresourceCount; ++i)
                    {
                        if (latestUsages[i] != usage)
                        {
                            CompiledTextureBarrier barrier = { handle, latestUsages[i], usage, {i, 0, 0, 0, 0, 0} };
                            node.textureBarriers.push_back(barrier);
                        }
                    }
                }
                else
                {
                    const auto [mipLevels, depthOrArraySize, planeCount] = GetResourceDimension(m_pDevice, handle);

                    for (UINT plane = FirstPlane; plane < FirstPlane + NumPlanes; ++plane)
                    {
                        for (UINT array = FirstArraySlice; array < FirstArraySlice + NumArraySlices; ++array)
                        {
                            for (UINT mip = IndexOrFirstMipLevel; mip < IndexOrFirstMipLevel + NumMipLevels; ++mip)
                            {
                                UINT subresourceIndex = CalcSubresourceIndex(mip, array, plane, mipLevels, depthOrArraySize);

                                if (latestUsages[subresourceIndex] != usage)
                                {
                                    CompiledTextureBarrier barrier = { handle, latestUsages[subresourceIndex], usage, {subresourceIndex, 0, 0, 0, 0, 0} };
                                    node.textureBarriers.push_back(barrier);
                                }
                            }
                        }
                    }
                }
            }

            // Process buffer outputs
            for (auto& [handle, usage] : node.bufferOutputs)
            {
                currentBufferUsages[handle.index] = usage;
            }

            // Process texture outputs
            for (auto& [handle, usage, range] : node.textureOutputs)
            {
                auto& latestUsages = currentTextureUsages[handle.index];

                const auto& [IndexOrFirstMipLevel, NumMipLevels, FirstArraySlice, NumArraySlices, FirstPlane, NumPlanes] = range;

                if (IndexOrFirstMipLevel == 0xffff'ffff && NumMipLevels == 0)
                {
                    UINT subresourceCount = D3DHelper::GetSubresourceCount(m_pDevice, m_textureGroups[handle.index].pResources.front());

                    for (UINT i = 0; i < subresourceCount; ++i)
                    {
                        latestUsages[i] = usage;
                    }
                }
                else
                {
                    const auto [mipLevels, depthOrArraySize, planeCount] = GetResourceDimension(m_pDevice, handle);

                    for (UINT plane = FirstPlane; plane < FirstPlane + NumPlanes; ++plane)
                    {
                        for (UINT array = FirstArraySlice; array < FirstArraySlice + NumArraySlices; ++array)
                        {
                            for (UINT mip = IndexOrFirstMipLevel; mip < IndexOrFirstMipLevel + NumMipLevels; ++mip)
                            {
                                UINT subresourceIndex = CalcSubresourceIndex(mip, array, plane, mipLevels, depthOrArraySize);

                                if (latestUsages[subresourceIndex] != usage)
                                {
                                    latestUsages[subresourceIndex] = usage;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    const std::vector<CompiledBufferBarrier>& GetCompiledBufferBarriers(PassType passType) const
    {
        return m_nodes[static_cast<UINT>(passType)].bufferBarriers;
    }

    const std::vector<CompiledTextureBarrier>& GetCompiledTextureBarrier(PassType passType) const
    {
        return m_nodes[static_cast<UINT>(passType)].textureBarriers;
    }

    std::array<RenderGraphNode, static_cast<std::size_t>(PassType::NUM_PASS_TYPES)> m_nodes;

private:
    struct ResourceGroup
    {
        std::vector<ID3D12Resource*> pResources;    // size = elementCount * (isPerFrame ? frameCount : 1)
        UINT elementCount;
        bool isPerFrame;

        TextureResourceUsage initialUsage;
    };

    UINT RegisterHelper(
        const std::string& name,
        bool isPerFrame,
        std::vector<ResourceGroup>& groups,
        std::unordered_map<std::string, UINT>& map,
        TextureResourceUsage initialUsage)
    {
        UINT ret = static_cast<UINT>(groups.size());

        ResourceGroup newEntry;
        newEntry.elementCount = 0;
        newEntry.isPerFrame = isPerFrame;
        newEntry.initialUsage = initialUsage;
        groups.push_back(newEntry);

        map[name] = ret;

        return ret;
    }

    void AddElementHelper(
        UINT index,
        std::vector<ID3D12Resource*>& pResources,
        std::vector<ResourceGroup>& groups)
    {
        auto& group = groups[index];
        group.pResources.insert(group.pResources.end(), pResources.begin(), pResources.end());
        ++group.elementCount;
    }

    void UpdateElementHelper(
        UINT index,
        UINT elementIndex,
        std::vector<ID3D12Resource*>& pResources,
        std::vector<ResourceGroup>& groups)
    {
        auto& group = groups[index];
        UINT offset = group.isPerFrame ? elementIndex * m_frameCount : elementIndex;
        std::copy(pResources.begin(), pResources.end(), group.pResources.begin() + offset);
    }

    ID3D12Resource* ResolveHelper(
        UINT index,
        UINT elementIndex,
        UINT frameIndex,
        const std::vector<ResourceGroup>& groups) const
    {
        assert(elementIndex < groups[index].elementCount && frameIndex < m_frameCount);

        bool isPerFrame = groups[index].isPerFrame;
        UINT idx = isPerFrame ? elementIndex * m_frameCount + frameIndex : elementIndex;

        return groups[index].pResources[idx];
    }

    std::vector<ResourceGroup> m_bufferGroups;
    std::vector<ResourceGroup> m_textureGroups;
    std::unordered_map<std::string, UINT> m_bufferMap;
    std::unordered_map<std::string, UINT> m_textureMap;
    UINT m_frameCount;

    ID3D12Device10* m_pDevice;
};