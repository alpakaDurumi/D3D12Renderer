#pragma once

#include <d3d12.h>

#include <vector>
#include <utility>
#include <tuple>

#include "Aliases.h"

struct BufferHandle { UINT index; };
struct TextureHandle { UINT index; };

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
    BufferHandle handle;
    BufferResourceUsage before;
    BufferResourceUsage after;
};

struct CompiledTextureBarrier
{
    TextureHandle handle;
    TextureResourceUsage before;
    TextureResourceUsage after;
    D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange;
};

struct RenderGraphNode
{
public:
    void AddBufferInput(BufferHandle handle, BufferResourceUsage usage)
    {
        bufferInputs.emplace_back(handle, usage);
    }

    void AddBufferOutput(BufferHandle handle, BufferResourceUsage usage)
    {
        bufferOutputs.emplace_back(handle, usage);
    }

    void AddTextureInput(TextureHandle handle, TextureResourceUsage usage, D3D12_BARRIER_SUBRESOURCE_RANGE range = { 0xffff'ffff, 0, 0, 0, 0, 0 })
    {
        textureInputs.emplace_back(handle, usage, range);
    }

    void AddTextureOutput(TextureHandle handle, TextureResourceUsage usage, D3D12_BARRIER_SUBRESOURCE_RANGE range = { 0xffff'ffff, 0, 0, 0, 0, 0 })
    {
        textureOutputs.emplace_back(handle, usage, range);
    }

    std::vector<std::pair<BufferHandle, BufferResourceUsage>> bufferInputs;
    std::vector<std::pair<BufferHandle, BufferResourceUsage>> bufferOutputs;

    std::vector<std::tuple<TextureHandle, TextureResourceUsage, D3D12_BARRIER_SUBRESOURCE_RANGE>> textureInputs;
    std::vector<std::tuple<TextureHandle, TextureResourceUsage, D3D12_BARRIER_SUBRESOURCE_RANGE>> textureOutputs;

    std::vector<CompiledBufferBarrier> bufferBarriers;
    std::vector<CompiledTextureBarrier> textureBarriers;
};