#pragma once

#include <d3d12.h>

#include <vector>
#include <utility>
#include <tuple>

#include "Aliases.h"

struct BufferResourceUsage
{
    D3D12_BARRIER_SYNC sync;
    D3D12_BARRIER_ACCESS access;

    bool operator==(const BufferResourceUsage& other) const
    {
        return this->sync == other.sync &&
            this->access == other.access;
    }

    bool operator!=(const BufferResourceUsage& other) const
    {
        return !(*this == other);
    }
};

struct TextureResourceUsage
{
    D3D12_BARRIER_SYNC sync;
    D3D12_BARRIER_ACCESS access;
    D3D12_BARRIER_LAYOUT layout;

    bool operator==(const TextureResourceUsage& other) const
    {
        return this->sync == other.sync &&
            this->access == other.access &&
            this->layout == other.layout;
    }

    bool operator!=(const TextureResourceUsage& other) const
    {
        return !(*this == other);
    }
};

struct CompiledBufferBarrier
{
    ResourceHandle handle;
    BufferResourceUsage before;
    BufferResourceUsage after;
};

struct CompiledTextureBarrier
{
    ResourceHandle handle;
    TextureResourceUsage before;
    TextureResourceUsage after;
    D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange;
};

struct RenderGraphNode
{
public:
    void AddBufferInput(ResourceHandle handle, BufferResourceUsage usage)
    {
        bufferInputs.emplace_back(handle, usage);
    }

    void AddBufferOutput(ResourceHandle handle, BufferResourceUsage usage)
    {
        bufferOutputs.emplace_back(handle, usage);
    }

    void AddTextureInput(ResourceHandle handle, TextureResourceUsage usage, D3D12_BARRIER_SUBRESOURCE_RANGE range = { 0xffff'ffff, 0, 0, 0, 0, 0 })
    {
        textureInputs.emplace_back(handle, usage, range);
    }

    void AddTextureOutput(ResourceHandle handle, TextureResourceUsage usage, D3D12_BARRIER_SUBRESOURCE_RANGE range = { 0xffff'ffff, 0, 0, 0, 0, 0 })
    {
        textureOutputs.emplace_back(handle, usage, range);
    }

    std::vector<std::pair<ResourceHandle, BufferResourceUsage>> bufferInputs;
    std::vector<std::pair<ResourceHandle, BufferResourceUsage>> bufferOutputs;

    std::vector<std::tuple<ResourceHandle, TextureResourceUsage, D3D12_BARRIER_SUBRESOURCE_RANGE>> textureInputs;
    std::vector<std::tuple<ResourceHandle, TextureResourceUsage, D3D12_BARRIER_SUBRESOURCE_RANGE>> textureOutputs;

    std::vector<CompiledBufferBarrier> bufferBarriers;
    std::vector<CompiledTextureBarrier> textureBarriers;
};