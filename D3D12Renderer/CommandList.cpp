#include "CommandList.h"

#include "D3DHelper.h"

#include <cassert>

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

void CommandList::Barrier(
    ID3D12Resource* pResource,
    ResourceLayoutTracker& layoutTracker,
    D3D12_BARRIER_SYNC syncBefore,
    D3D12_BARRIER_SYNC syncAfter,
    D3D12_BARRIER_ACCESS accessBefore,
    D3D12_BARRIER_ACCESS accessAfter,
    D3D12_BARRIER_LAYOUT layoutAfter,
    D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange)
{
    D3D12_RESOURCE_DESC desc = pResource->GetDesc();
    assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ||
        desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
        desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D);

    // subresourceRange를 순회하며 레이아웃을 확인하여 배열에 추가한 후, 배리어 그룹을 구성

    const auto& [IndexOrFirstMipLevel, NumMipLevels, FirstArraySlice, NumArraySlices, FirstPlane, NumPlanes] = subresourceRange;

    UINT count = desc.MipLevels * desc.DepthOrArraySize * GetFormatPlaneCount(m_device.Get(), desc.Format);

    std::vector<D3D12_TEXTURE_BARRIER> barriers;

    auto it = m_latestLayouts.find(pResource);
    if (it == m_latestLayouts.end())
    {
        it = m_latestLayouts.try_emplace(pResource, desc.MipLevels, desc.DepthOrArraySize, GetFormatPlaneCount(m_device.Get(), desc.Format), D3D12_BARRIER_LAYOUT_COMMON).first;
    }

    auto& [layoutInfo, isNotUsed] = it->second;

    // Target all subresources
    if (IndexOrFirstMipLevel == 0xffffffff)
    {
        for (UINT i = 0; i < count; i++)
        {
            bool toPending = true;
            D3D12_BARRIER_LAYOUT layoutBefore;
            if (isNotUsed[i])
            {
                layoutBefore = D3D12_BARRIER_LAYOUT_COMMON;
                isNotUsed[i] = false;   // 사용함으로 표시
            }
            else
            {
                layoutBefore = layoutInfo.GetLayout(i);
                toPending = false;
            }

            D3D12_TEXTURE_BARRIER barrier =
            {
                syncBefore,
                syncAfter,
                accessBefore,
                accessAfter,
                layoutBefore,
                layoutAfter,
                pResource,
                {i, 0, 0, 0, 0, 0},
                D3D12_TEXTURE_BARRIER_FLAG_NONE
            };

            layoutInfo.SetLayout(i, layoutAfter);

            if (toPending)
            {
                m_pendingBarriers.push_back(barrier);
            }
            else
            {
                barriers.push_back(barrier);
            }
        }
    }
    else
    {
        for (UINT plane = FirstPlane; plane < FirstPlane + NumPlanes; ++plane)
        {
            for (UINT array = FirstArraySlice; array < FirstArraySlice + NumArraySlices; ++array)
            {
                for (UINT mip = IndexOrFirstMipLevel; mip < IndexOrFirstMipLevel + NumMipLevels; ++mip)
                {
                    UINT subresourceIndex = CalcSubresourceIndex(mip, array, plane, desc.MipLevels, desc.DepthOrArraySize);

                    bool toPending = true;
                    D3D12_BARRIER_LAYOUT layoutBefore;
                    if (isNotUsed[subresourceIndex])
                    {
                        layoutBefore = D3D12_BARRIER_LAYOUT_COMMON;
                        isNotUsed[subresourceIndex] = false;
                    }
                    else
                    {
                        layoutBefore = layoutInfo.GetLayout(subresourceIndex);
                        toPending = false;
                    }

                    D3D12_TEXTURE_BARRIER barrier =
                    {
                        syncBefore,
                        syncAfter,
                        accessBefore,
                        accessAfter,
                        layoutBefore,
                        layoutAfter,
                        pResource,
                        {subresourceIndex, 0, 0, 0, 0, 0},
                        D3D12_TEXTURE_BARRIER_FLAG_NONE
                    };

                    layoutInfo.SetLayout(subresourceIndex, layoutAfter);

                    if (toPending)
                    {
                        m_pendingBarriers.push_back(barrier);
                    }
                    else
                    {
                        barriers.push_back(barrier);
                    }
                }
            }
        }
    }

    if (!barriers.empty())
    {
        D3D12_BARRIER_GROUP barrierGroups[] = { TextureBarrierGroup(UINT32(barriers.size()), barriers.data()) };
        m_commandList->Barrier(1, barrierGroups);
    }
}