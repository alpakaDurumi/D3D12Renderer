#pragma once

#include <Windows.h>

#include <d3d12.h>

#include <vector>
#include <array>
#include <cassert>
#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <functional>

#include "RenderGraphNode.h"
#include "CacheKeys.h"
#include "D3DHelper.h"

class RenderGraph
{
public:
    void Init(ID3D12Device10* pDevice, UINT frameCount)
    {
        m_pDevice = pDevice;
        SetFrameCount(frameCount);
    }

    // Each register functions have two versions: Static list vs Dynamic list
    // Latter needs provider function that dynamically provides actual list
    RGBuffer RegisterBuffer(
        const std::string& name,
        bool isPerFrame)
    {
        auto idx = RegisterHelper(name, isPerFrame, m_bufferGroups, m_bufferMap, {});
        m_bufferGroups[idx].isDynamic = false;
        return { idx };
    }

    RGBuffer RegisterBuffer(
        const std::string& name,
        bool isPerFrame,
        std::function<std::vector<ID3D12Resource*>()> provider)
    {
        auto idx = RegisterHelper(name, isPerFrame, m_bufferGroups, m_bufferMap, {});
        m_bufferGroups[idx].isDynamic = true;
        m_bufferGroups[idx].provider = provider;
        return { idx };
    }

    RGTexture RegisterTexture(
        const std::string& name,
        bool isPerFrame,
        TextureResourceUsage initialUsage,
        UINT subresourceCount)
    {
        auto idx = RegisterHelper(name, isPerFrame, m_textureGroups, m_textureMap, initialUsage);
        m_textureGroups[idx].isDynamic = false;
        m_textureGroups[idx].subresourceCount = subresourceCount;
        return { idx };
    }

    RGTexture RegisterTexture(
        const std::string& name,
        bool isPerFrame,
        TextureResourceUsage initialUsage,
        UINT subresourceCount,
        std::function<std::vector<ID3D12Resource*>()> provider)
    {
        auto idx = RegisterHelper(name, isPerFrame, m_textureGroups, m_textureMap, initialUsage);
        m_textureGroups[idx].isDynamic = true;
        m_textureGroups[idx].subresourceCount = subresourceCount;
        m_textureGroups[idx].provider = provider;
        return { idx };
    }

    std::vector<ID3D12Resource*> GetResources(RGBuffer buffer, UINT frameIndex = 0) const
    {
        auto& group = m_bufferGroups[buffer.index];

        if (group.isDynamic)
        {
            return group.provider();
        }
        else
        {
            UINT elementCount = GetElementCount(buffer);
            std::vector<ID3D12Resource*> ret(elementCount);
            for (UINT i = 0; i < elementCount; ++i)
                ret[i] = Resolve(buffer, i, frameIndex);
            return ret;
        }
    }

    std::vector<ID3D12Resource*> GetResources(RGTexture texture, UINT frameIndex = 0) const
    {
        auto& group = m_textureGroups[texture.index];

        if (group.isDynamic)
        {
            return group.provider();
        }
        else
        {
            UINT elementCount = GetElementCount(texture);
            std::vector<ID3D12Resource*> ret(elementCount);
            for (UINT i = 0; i < elementCount; ++i)
                ret[i] = Resolve(texture, i, frameIndex);
            return ret;
        }
    }

    void AddElement(RGBuffer buffer, std::vector<ID3D12Resource*> pResources)
    {
        AddElementHelper(buffer.index, pResources, m_bufferGroups);
    }

    void AddElement(RGTexture texture, std::vector<ID3D12Resource*> pResources)
    {
        AddElementHelper(texture.index, pResources, m_textureGroups);
    }

    void UpdateElement(RGBuffer buffer, UINT elementIndex, std::vector<ID3D12Resource*> pResources)
    {
        UpdateElementHelper(buffer.index, elementIndex, pResources, m_bufferGroups);
    }

    void UpdateElement(RGTexture texture, UINT elementIndex, std::vector<ID3D12Resource*> pResources)
    {
        UpdateElementHelper(texture.index, elementIndex, pResources, m_textureGroups);
    }

    ID3D12Resource* Resolve(RGBuffer buffer, UINT elementIndex, UINT frameIndex) const
    {
        return ResolveHelper(buffer.index, elementIndex, frameIndex, m_bufferGroups);
    }

    ID3D12Resource* Resolve(RGTexture texture, UINT elementIndex, UINT frameIndex) const
    {
        return ResolveHelper(texture.index, elementIndex, frameIndex, m_textureGroups);
    }

    std::tuple<UINT, UINT, UINT> GetResourceDimension(ID3D12Device* pDevice, RGTexture texture) const
    {
        auto desc = m_textureGroups[texture.index].pResources.front()->GetDesc();
        UINT8 planeCount = D3DHelper::GetFormatPlaneCount(pDevice, desc.Format);
        return { desc.MipLevels, desc.DepthOrArraySize, planeCount };
    }

    void SetFrameCount(UINT frameCount)
    {
        m_frameCount = frameCount;
    }

    UINT GetElementCount(RGBuffer buffer) const
    {
        return m_bufferGroups[buffer.index].elementCount;
    }

    UINT GetElementCount(RGTexture texture) const
    {
        return m_textureGroups[texture.index].elementCount;
    }

    RGBuffer GetRGBuffer(const std::string& name)
    {
        return { m_bufferMap.at(name) };
    }

    RGTexture GetRGTexture(const std::string& name)
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
            UINT subresourceCount = group.subresourceCount;
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
            for (auto& [buffer, usage] : node.bufferInputs)
            {
                CompiledBufferBarrier barrier = { buffer, currentBufferUsages[buffer.index], usage };
                node.bufferBarriers.push_back(barrier);
            }

            // Process texture inputs
            for (auto& [texture, usage, range] : node.textureInputs)
            {
                auto& latestUsages = currentTextureUsages[texture.index];

                const auto& [IndexOrFirstMipLevel, NumMipLevels, FirstArraySlice, NumArraySlices, FirstPlane, NumPlanes] = range;

                if (IndexOrFirstMipLevel == 0xffff'ffff && NumMipLevels == 0)
                {
                    UINT subresourceCount = m_textureGroups[texture.index].subresourceCount;

                    for (UINT i = 0; i < subresourceCount; ++i)
                    {
                        if (latestUsages[i] != usage)
                        {
                            CompiledTextureBarrier barrier = { texture, latestUsages[i], usage, {i, 0, 0, 0, 0, 0} };
                            node.textureBarriers.push_back(barrier);
                        }
                    }
                }
                else
                {
                    const auto [mipLevels, depthOrArraySize, planeCount] = GetResourceDimension(m_pDevice, texture);

                    for (UINT plane = FirstPlane; plane < FirstPlane + NumPlanes; ++plane)
                    {
                        for (UINT array = FirstArraySlice; array < FirstArraySlice + NumArraySlices; ++array)
                        {
                            for (UINT mip = IndexOrFirstMipLevel; mip < IndexOrFirstMipLevel + NumMipLevels; ++mip)
                            {
                                UINT subresourceIndex = CalcSubresourceIndex(mip, array, plane, mipLevels, depthOrArraySize);

                                if (latestUsages[subresourceIndex] != usage)
                                {
                                    CompiledTextureBarrier barrier = { texture, latestUsages[subresourceIndex], usage, {subresourceIndex, 0, 0, 0, 0, 0} };
                                    node.textureBarriers.push_back(barrier);
                                }
                            }
                        }
                    }
                }
            }

            // Process buffer outputs
            for (auto& [buffer, usage] : node.bufferOutputs)
            {
                currentBufferUsages[buffer.index] = usage;
            }

            // Process texture outputs
            for (auto& [texture, usage, range] : node.textureOutputs)
            {
                auto& latestUsages = currentTextureUsages[texture.index];

                const auto& [IndexOrFirstMipLevel, NumMipLevels, FirstArraySlice, NumArraySlices, FirstPlane, NumPlanes] = range;

                if (IndexOrFirstMipLevel == 0xffff'ffff && NumMipLevels == 0)
                {
                    UINT subresourceCount = m_textureGroups[texture.index].subresourceCount;

                    for (UINT i = 0; i < subresourceCount; ++i)
                    {
                        latestUsages[i] = usage;
                    }
                }
                else
                {
                    const auto [mipLevels, depthOrArraySize, planeCount] = GetResourceDimension(m_pDevice, texture);

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
        bool isDynamic;

        UINT subresourceCount;

        TextureResourceUsage initialUsage;

        std::function<std::vector<ID3D12Resource*>()> provider;
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