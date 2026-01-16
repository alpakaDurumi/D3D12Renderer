#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include "D3DHelper.h"
#include "ConstantData.h"
#include "DescriptorAllocation.h"
#include "ResourceLayoutTracker.h"
#include "SharedConfig.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

class Light
{
public:
    // TODO : Implement constructor and add fields (pos, dir, etc...) and getter functions
    Light(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        UINT shadowMapResolution,
        ResourceLayoutTracker& layoutTracker,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources,
        DescriptorAllocation&& cbvAllocation)
        : m_shadowMapDsvAllocation(std::move(dsvAllocation)),
        m_shadowMapSrvAllocation(std::move(srvAllocation))
    {
        assert(!m_shadowMapDsvAllocation.IsNull() && !m_shadowMapSrvAllocation.IsNull() && cbvAllocation.GetNumHandles() == frameResources.size());

        CreateShadowMap(pDevice, shadowMapResolution, shadowMapResolution, m_shadowMap, m_shadowMapDsvAllocation, m_shadowMapSrvAllocation.GetDescriptorHandle());
        layoutTracker.RegisterResource(m_shadowMap.Get(), D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, MAX_CASCADES, 1, DXGI_FORMAT_R32_TYPELESS);

        auto cbvAllocations = cbvAllocation.Split();

        // Create constant buffers
        for (UINT i = 0; i < frameResources.size(); ++i)
        {
            FrameResource& frameResource = *frameResources[i];

            if (i == 0) m_lightConstantBufferIndex = UINT(frameResource.m_lightConstantBuffers.size());
            frameResource.m_lightConstantBuffers.push_back(std::make_unique<LightCB>(pDevice, std::move(cbvAllocations[i])));

            for (UINT j = 0; j < MAX_CASCADES; ++j)
            {
                if (i == 0) m_cameraConstantBufferIndex[j] = UINT(frameResource.m_cameraConstantBuffers.size());
                frameResource.m_cameraConstantBuffers.push_back(std::make_unique<CameraCB>(pDevice));
            }
        }
    }

    void UpdateCameraConstantBuffer(FrameResource& frameResource, UINT idx)
    {
        frameResource.m_cameraConstantBuffers[m_cameraConstantBufferIndex[idx]]->Update(&m_cameraConstantData[idx]);
    }

    void UpdateLightConstantBuffer(FrameResource& frameResource)
    {
        frameResource.m_lightConstantBuffers[m_lightConstantBufferIndex]->Update(&m_lightConstantData);
    }

public:
    CameraConstantData m_cameraConstantData[MAX_CASCADES];
    UINT m_cameraConstantBufferIndex[MAX_CASCADES];

    LightConstantData m_lightConstantData;
    UINT m_lightConstantBufferIndex;

    ComPtr<ID3D12Resource> m_shadowMap;
    DescriptorAllocation m_shadowMapDsvAllocation;
    DescriptorAllocation m_shadowMapSrvAllocation;
};