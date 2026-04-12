#pragma once

#include <d3d12.h>

#include <vector>
#include <utility>
#include <tuple>

struct RGBuffer { UINT index; };
struct RGTexture { UINT index; };

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
    RGBuffer buffer;
    BufferResourceUsage before;
    BufferResourceUsage after;
};

struct CompiledTextureBarrier
{
    RGTexture texture;
    TextureResourceUsage before;
    TextureResourceUsage after;
    D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange;
};

struct RenderGraphNode
{
public:
    void AddBufferInput(RGBuffer buffer, BufferResourceUsage usage)
    {
        bufferInputs.emplace_back(buffer, usage);
    }

    void AddBufferOutput(RGBuffer buffer, BufferResourceUsage usage)
    {
        bufferOutputs.emplace_back(buffer, usage);
    }

    void AddTextureInput(RGTexture texture, TextureResourceUsage usage, D3D12_BARRIER_SUBRESOURCE_RANGE range = { 0xffff'ffff, 0, 0, 0, 0, 0 })
    {
        textureInputs.emplace_back(texture, usage, range);
    }

    void AddTextureOutput(RGTexture texture, TextureResourceUsage usage, D3D12_BARRIER_SUBRESOURCE_RANGE range = { 0xffff'ffff, 0, 0, 0, 0, 0 })
    {
        textureOutputs.emplace_back(texture, usage, range);
    }

    std::vector<std::pair<RGBuffer, BufferResourceUsage>> bufferInputs;
    std::vector<std::pair<RGBuffer, BufferResourceUsage>> bufferOutputs;

    std::vector<std::tuple<RGTexture, TextureResourceUsage, D3D12_BARRIER_SUBRESOURCE_RANGE>> textureInputs;
    std::vector<std::tuple<RGTexture, TextureResourceUsage, D3D12_BARRIER_SUBRESOURCE_RANGE>> textureOutputs;

    std::vector<CompiledBufferBarrier> bufferBarriers;
    std::vector<CompiledTextureBarrier> textureBarriers;
};