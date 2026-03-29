#pragma once

#include <d3d12.h>

#include <vector>
#include <tuple>

#include "Aliases.h"

struct ResourceUsage
{
    D3D12_BARRIER_SYNC sync;
    D3D12_BARRIER_ACCESS access;
    D3D12_BARRIER_LAYOUT layout;

    bool operator==(const ResourceUsage& other)
    {
        return this->sync == other.sync &&
            this->access == other.access &&
            this->layout == other.layout;
    }

    bool operator!=(const ResourceUsage& other)
    {
        return !(*this == other);
    }
};

struct CompiledBarrier
{
    ResourceHandle handle;
    ResourceUsage before;
    ResourceUsage after;
    D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange;
};

struct RenderGraphNode
{
public:
    void AddInput(ResourceHandle handle, ResourceUsage usage, D3D12_BARRIER_SUBRESOURCE_RANGE range = { 0xffff'ffff, 0, 0, 0, 0, 0 })
    {
        inputs.emplace_back(handle, usage, range);
    }

    void AddOutput(ResourceHandle handle, ResourceUsage usage, D3D12_BARRIER_SUBRESOURCE_RANGE range = { 0xffff'ffff, 0, 0, 0, 0, 0 })
    {
        outputs.emplace_back(handle, usage, range);
    }

    std::vector<std::tuple<ResourceHandle, ResourceUsage, D3D12_BARRIER_SUBRESOURCE_RANGE>> inputs;
    std::vector<std::tuple<ResourceHandle, ResourceUsage, D3D12_BARRIER_SUBRESOURCE_RANGE>> outputs;

    std::vector<CompiledBarrier> barriers;
};