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
protected:
    Light(DescriptorAllocation&& dsvAllocation, DescriptorAllocation&& srvAllocation, UINT arraySize, LightType type)
        : m_shadowMapDsvAllocation(std::move(dsvAllocation)),
        m_shadowMapSrvAllocation(std::move(srvAllocation)),
        m_cameraConstantData(arraySize),
        m_cameraConstantBufferIndex(arraySize),
        m_type(type)
    {
        assert(!m_shadowMapDsvAllocation.IsNull() && !m_shadowMapSrvAllocation.IsNull());

        m_lightConstantData.type = static_cast<UINT32>(m_type);
    }

    void Init(
        ID3D12Device10* pDevice,
        UINT shadowMapResolution,
        ResourceLayoutTracker& layoutTracker,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources,
        DescriptorAllocation&& cbvAllocation,
        UINT16 arraySize)
    {
        assert(cbvAllocation.GetNumHandles() == frameResources.size());

        CreateShadowMap(pDevice, shadowMapResolution, shadowMapResolution, m_shadowMap, m_shadowMapDsvAllocation, m_shadowMapSrvAllocation.GetDescriptorHandle(), arraySize);
        layoutTracker.RegisterResource(m_shadowMap.Get(), D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, arraySize, 1, DXGI_FORMAT_R32_TYPELESS);

        auto cbvAllocations = cbvAllocation.Split();

        // Create constant buffers
        for (UINT i = 0; i < frameResources.size(); ++i)
        {
            FrameResource& frameResource = *frameResources[i];

            if (i == 0) m_lightConstantBufferIndex = UINT(frameResource.m_lightConstantBuffers.size());
            frameResource.m_lightConstantBuffers.push_back(std::make_unique<LightCB>(pDevice, std::move(cbvAllocations[i])));

            for (UINT j = 0; j < arraySize; ++j)
            {
                if (i == 0) m_cameraConstantBufferIndex[j] = UINT(frameResource.m_cameraConstantBuffers.size());
                frameResource.m_cameraConstantBuffers.push_back(std::make_unique<CameraCB>(pDevice));
            }
        }
    }

public:
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

    UINT16 GetArraySize() const
    {
        switch (m_type)
        {
        case LightType::DIRECTIONAL:
            return MAX_CASCADES;
        case LightType::POINT:
            return POINT_LIGHT_ARRAY_SIZE;
        case LightType::SPOT:
            return SPOT_LIGHT_ARRAY_SIZE;
        default:
            return -1;
        }
    }

    virtual XMVECTOR GetPosition() const
    {
        XMVECTOR p = XMLoadFloat3(&m_lightConstantData.lightPos);
        return XMVectorSetW(p, 1.0f);
    }

    virtual XMVECTOR GetDirection() const
    {
        return XMLoadFloat3(&m_lightConstantData.lightDir);
    }

    virtual float GetRange() const
    {
        return m_lightConstantData.range;
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

    virtual void SetRange(float range)
    {
        m_lightConstantData.range = range;
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
    std::vector<CameraConstantData> m_cameraConstantData;
    std::vector<UINT> m_cameraConstantBufferIndex;

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
        : Light(std::move(dsvAllocation), std::move(srvAllocation), MAX_CASCADES, LightType::DIRECTIONAL)
    {
        Init(pDevice, shadowMapResolution, layoutTracker, frameResources, std::move(cbvAllocation), MAX_CASCADES);
    }

    virtual XMVECTOR GetPosition() const override
    {
        assert(false);
        return XMVectorZero();
    }

    virtual float GetRange() const override
    {
        assert(false);
        return 0.0f;
    }

    void SetPosition(XMFLOAT3 pos) override
    {
        assert(false);
    }

    void SetPosition(XMVECTOR pos) override
    {
        assert(false);
    }

    virtual void SetRange(float range) override
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
        : Light(std::move(dsvAllocation), std::move(srvAllocation), POINT_LIGHT_ARRAY_SIZE, LightType::POINT)
    {
        Init(pDevice, shadowMapResolution, layoutTracker, frameResources, std::move(cbvAllocation), POINT_LIGHT_ARRAY_SIZE);
    }

    XMVECTOR GetDirection() const override
    {
        assert(false);
        return XMVectorZero();
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
        : Light(std::move(dsvAllocation), std::move(srvAllocation), SPOT_LIGHT_ARRAY_SIZE, LightType::SPOT)
    {
        Init(pDevice, shadowMapResolution, layoutTracker, frameResources, std::move(cbvAllocation), SPOT_LIGHT_ARRAY_SIZE);
    }

    void SetAngle(float angle)
    {
        m_lightConstantData.angle = angle;
    }
};