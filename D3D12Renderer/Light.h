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

enum class LightType
{
    DIRECTIONAL,
    POINT,
    SPOT,
    NUM_LIGHT_TYPES
};

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

    LightType GetType() const
    {
        return m_type;
    }

    ID3D12Resource* GetShadowMap() const
    {
        return m_shadowMap.Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVDescriptorHandle(UINT idx) const
    {
        return m_shadowMapDsvAllocation.GetDescriptorHandle(idx);
    }

    UINT GetCameraConstantBufferIndex(UINT idx) const
    {
        return m_cameraConstantBufferIndex[idx];
    }

    UINT GetLightConstantBufferIndex() const
    {
        return m_lightConstantBufferIndex;
    }

    DescriptorAllocation& GetSRVAllocationRef()
    {
        return m_shadowMapSrvAllocation;
    }

    virtual XMVECTOR GetDirection() const
    {
        return XMLoadFloat3(&m_lightConstantData.lightDir);
    }

    virtual void SetPosition(XMFLOAT3 pos)
    {
        XMVECTOR p = XMLoadFloat3(&pos);
        m_lightConstantData.SetPos(p);
    }

    virtual void SetPosition(XMVECTOR pos)
    {
        m_lightConstantData.SetPos(pos);
    }

    virtual void SetDirection(XMFLOAT3 dir)
    {
        XMVECTOR d = XMLoadFloat3(&dir);
        m_lightConstantData.SetLightDir(d);
    }

    virtual void SetDirection(XMVECTOR dir)
    {
        m_lightConstantData.SetLightDir(dir);
    }

    void SetViewProjection(XMMATRIX view, XMMATRIX projection, UINT idx)
    {
        m_cameraConstantData[idx].SetView(view);
        m_cameraConstantData[idx].SetProjection(projection);
        m_lightConstantData.SetViewProjection(view * projection, idx);
    }

    void UpdateCameraConstantBuffer(FrameResource& frameResource, UINT idx)
    {
        frameResource.m_cameraConstantBuffers[m_cameraConstantBufferIndex[idx]]->Update(&m_cameraConstantData[idx]);
    }

    void UpdateLightConstantBuffer(FrameResource& frameResource)
    {
        frameResource.m_lightConstantBuffers[m_lightConstantBufferIndex]->Update(&m_lightConstantData);
    }

protected:
    CameraConstantData m_cameraConstantData[MAX_CASCADES];
    UINT m_cameraConstantBufferIndex[MAX_CASCADES];

    LightConstantData m_lightConstantData;
    UINT m_lightConstantBufferIndex;

    LightType m_type;

    ComPtr<ID3D12Resource> m_shadowMap;
    DescriptorAllocation m_shadowMapDsvAllocation;
    DescriptorAllocation m_shadowMapSrvAllocation;
};

class DirectionalLight : public Light
{
public:
    DirectionalLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        UINT shadowMapResolution,
        ResourceLayoutTracker& layoutTracker,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources,
        DescriptorAllocation&& cbvAllocation)
        : Light(
            pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            shadowMapResolution,
            layoutTracker,
            frameResources,
            std::move(cbvAllocation))
    {
        m_type = LightType::DIRECTIONAL;
    }

    void SetPosition(XMFLOAT3 pos) override
    {
        assert(false);
    }

    void SetPosition(XMVECTOR pos) override
    {
        assert(false);
    }
};

class PointLight : public Light
{
public:
    PointLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        UINT shadowMapResolution,
        ResourceLayoutTracker& layoutTracker,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources,
        DescriptorAllocation&& cbvAllocation)
        : Light(
            pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            shadowMapResolution,
            layoutTracker,
            frameResources,
            std::move(cbvAllocation))
    {
        m_type = LightType::POINT;
    }

    XMVECTOR GetDirection() const override
    {
        assert(false);
    }

    void SetDirection(XMFLOAT3 dir) override
    {
        assert(false);
    }

    void SetDirection(XMVECTOR dir) override
    {
        assert(false);
    }
};

class SpotLight : public Light
{
public:
    SpotLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        UINT shadowMapResolution,
        ResourceLayoutTracker& layoutTracker,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources,
        DescriptorAllocation&& cbvAllocation)
        : Light(
            pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            shadowMapResolution,
            layoutTracker,
            frameResources,
            std::move(cbvAllocation))
    {
        m_type = LightType::SPOT;
    }

    void SetAngle(float angle)
    {
        m_lightConstantData.angle = angle;
    }
};