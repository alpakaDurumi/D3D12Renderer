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
    D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange;
    ResourceUsage before;
    ResourceUsage after;
};

struct RenderGraphNode
{
public:
    std::vector<std::tuple<ResourceHandle, D3D12_BARRIER_SUBRESOURCE_RANGE, ResourceUsage>> inputs;
    //std::vector<std::tuple<ResourceHandle, D3D12_BARRIER_SUBRESOURCE_RANGE, ResourceUsage>> outputs;

    std::vector<CompiledBarrier> barriers;
};