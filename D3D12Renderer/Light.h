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
        ResourceLayoutTracker& layoutTracker)
        : m_shadowMapDsvAllocation(std::move(dsvAllocation)),
        m_shadowMapSrvAllocation(std::move(srvAllocation))
    {
        CreateShadowMap(pDevice, shadowMapResolution, shadowMapResolution, m_shadowMap, m_shadowMapDsvAllocation, m_shadowMapSrvAllocation.GetDescriptorHandle());
        layoutTracker.RegisterResource(m_shadowMap.Get(), D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, MAX_CASCADES, 1, DXGI_FORMAT_R32_TYPELESS);
    }

    void UpdateCameraConstantBuffer(const FrameResource* pFrameResource, UINT idx)
    {
        pFrameResource->m_cameraConstantBuffers[m_cameraConstantBufferIndex[idx]]->Update(&m_cameraConstantData[idx]);
    }

    void UpdateLightConstantBuffer(const FrameResource* pFrameResource)
    {
        pFrameResource->m_lightConstantBuffers[m_lightConstantBufferIndex]->Update(&m_lightConstantData);
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