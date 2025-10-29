#pragma once

#include <wrl/client.h>
#include "d3d12.h"
#include <unordered_map>

#include "D3DHelper.h"
#include <cassert>

#include <vector>

#include "ResourceLayoutTracker.h"

using Microsoft::WRL::ComPtr;

using namespace D3DHelper;

// Wrapper for Command List. Only accessed by a single thread at a time.
class CommandList
{
public:
    void Barrier(
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

    void Barrier(
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

        bool isAdded = false;

        auto it = m_latestLayouts.find(pResource);
        if (it == m_latestLayouts.end())
        {
            m_latestLayouts.insert({ pResource, {
                ResourceLayoutInfo(desc.MipLevels, desc.DepthOrArraySize, GetFormatPlaneCount(m_device.Get(), desc.Format), D3D12_BARRIER_LAYOUT_COMMON),
                std::vector<bool>(count, true)
                }
                });
            isAdded = true;
        }

        // Target all subresources
        if (IndexOrFirstMipLevel == 0xffffffff)
        {
            for (UINT i = 0; i < count; i++)
            {
                bool toPending = true;
                D3D12_BARRIER_LAYOUT layoutBefore;
                if (isAdded || GetAndSetFirstUse(pResource, i))
                {
                    layoutBefore = D3D12_BARRIER_LAYOUT_COMMON;
                }
                else
                {
                    layoutBefore = m_latestLayouts[pResource].first.GetLayout(i);
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

                m_latestLayouts[pResource].first.SetLayout(i, layoutAfter);

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
                        //auto [layoutBefore, subresourceIndex] = layoutTracker.SetLayout(pResource, mip, array, plane, layoutAfter);
                        UINT subresourceIndex = CalcSubresourceIndex(mip, array, plane, desc.MipLevels, desc.DepthOrArraySize);

                        bool toPending = true;
                        D3D12_BARRIER_LAYOUT layoutBefore;
                        if (isAdded || GetAndSetFirstUse(pResource, subresourceIndex))
                        {
                            layoutBefore = D3D12_BARRIER_LAYOUT_COMMON;
                        }
                        else
                        {
                            layoutBefore = m_latestLayouts[pResource].first.GetLayout(subresourceIndex);
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

                        m_latestLayouts[pResource].first.SetLayout(subresourceIndex, layoutAfter);

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

        D3D12_BARRIER_GROUP barrierGroups[] = { TextureBarrierGroup(barriers.size(), barriers.data())};
        m_commandList->Barrier(1, barrierGroups);
    }

    bool GetAndSetFirstUse(ID3D12Resource* pResource, UINT subresourceIndex)
    {
        auto it = m_latestLayouts.find(pResource);
        assert(it != m_latestLayouts.end());
        bool result = m_latestLayouts[pResource].second[subresourceIndex];
        m_latestLayouts[pResource].second[subresourceIndex] = false;
        return result;
    }

private:
    ComPtr<ID3D12Device10> m_device;

    ComPtr<ID3D12GraphicsCommandList7> m_commandList;
    ComPtr<ID3D12GraphicsCommandList7> m_forSync;   // 이전 커맨드 리스트가 결정한 layout과의 동기화를 위해 m_commandList의 실행 이전 먼저 실행되는 command list
    std::unordered_map<ID3D12Resource*, std::pair<ResourceLayoutInfo, std::vector<bool>>> m_latestLayouts;  // 이번 command list 작성 중, 각 리소스에 대해 알려진 최신 layout
    std::vector<D3D12_TEXTURE_BARRIER> m_pendingBarriers;
};