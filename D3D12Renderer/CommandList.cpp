#include "pch.h"
#include "CommandList.h"

#include "D3DHelper.h"

using namespace D3DHelper;

void CommandList::Barrier(
    ID3D12Resource* pResource,
    D3D12_BARRIER_SYNC syncBefore,
    D3D12_BARRIER_SYNC syncAfter,
    D3D12_BARRIER_ACCESS accessBefore,
    D3D12_BARRIER_ACCESS accessAfter)
{
    D3D12_RESOURCE_DESC desc = pResource->GetDesc();
    assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);

    D3D12_BUFFER_BARRIER barrier =
    {
        syncBefore,
        syncAfter,
        accessBefore,
        accessAfter,
        pResource,
        0,
        UINT64_MAX
    };
    D3D12_BARRIER_GROUP barrierGroups[] = { BufferBarrierGroup(1, &barrier) };
    m_commandList->Barrier(1, barrierGroups);
}