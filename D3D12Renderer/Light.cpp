#include "pch.h"
#include "Light.h"

#include "D3DHelper.h"
#include "SharedConfig.h"

using namespace D3DHelper;

Light::Light(DescriptorAllocation&& dsvAllocation, DescriptorAllocation&& srvAllocation, LightType type)
    : m_shadowMapDsvAllocation(std::move(dsvAllocation)),
    m_shadowMapSrvAllocation(std::move(srvAllocation)),
    m_type(type)
{
    UINT16 arraySize = GetRequiredArraySize(type);

    assert(m_shadowMapDsvAllocation.GetNumHandles() == arraySize && !m_shadowMapSrvAllocation.IsNull());

    m_cameraConstantData.resize(arraySize);
    m_cameraConstantBufferIndex.resize(arraySize);

    m_lightConstantData.type = static_cast<UINT32>(m_type);
}

void Light::Init(
    ID3D12Device10* pDevice,
    UINT shadowMapResolution,
    ResourceLayoutTracker& layoutTracker,
    const std::vector<std::unique_ptr<FrameResource>>& frameResources,
    DescriptorAllocation&& cbvAllocation)
{
    assert(cbvAllocation.GetNumHandles() == frameResources.size());

    UINT16 arraySize = GetRequiredArraySize(m_type);

    CreateShadowMap(pDevice, shadowMapResolution, shadowMapResolution, m_shadowMap, m_shadowMapDsvAllocation, m_shadowMapSrvAllocation.GetDescriptorHandle(), m_type);
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

LightType Light::GetType() const
{
    return m_type;
}

ID3D12Resource* Light::GetShadowMap() const
{
    return m_shadowMap.Get();
}

UINT16 Light::GetArraySize() const
{
    return m_shadowMap->GetDesc().DepthOrArraySize;
}

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetDSVDescriptorHandle(UINT idx) const
{
    return m_shadowMapDsvAllocation.GetDescriptorHandle(idx);
}

UINT Light::GetCameraConstantBufferIndex(UINT idx) const
{
    return m_cameraConstantBufferIndex[idx];
}

UINT Light::GetLightConstantBufferIndex() const
{
    return m_lightConstantBufferIndex;
}

DescriptorAllocation& Light::GetSRVAllocationRef()
{
    return m_shadowMapSrvAllocation;
}

XMVECTOR Light::GetPosition() const
{
    XMVECTOR p = XMLoadFloat3(&m_lightConstantData.lightPos);
    return XMVectorSetW(p, 1.0f);
}

XMVECTOR Light::GetDirection() const
{
    return XMLoadFloat3(&m_lightConstantData.lightDir);
}

float Light::GetRange() const
{
    return m_lightConstantData.range;
}

void Light::SetPosition(XMFLOAT3 pos)
{
    XMVECTOR p = XMLoadFloat3(&pos);
    m_lightConstantData.SetPos(p);
}

void Light::SetPosition(XMVECTOR pos)
{
    m_lightConstantData.SetPos(pos);
}

void Light::SetDirection(XMFLOAT3 dir)
{
    XMVECTOR d = XMLoadFloat3(&dir);
    m_lightConstantData.SetLightDir(d);
}

void Light::SetDirection(XMVECTOR dir)
{
    m_lightConstantData.SetLightDir(dir);
}

void Light::SetRange(float range)
{
    m_lightConstantData.range = range;
}

void Light::SetViewProjection(XMMATRIX view, XMMATRIX projection, UINT idx)
{
    m_cameraConstantData[idx].SetView(view);
    m_cameraConstantData[idx].SetProjection(projection);
    m_lightConstantData.SetViewProjection(view * projection, idx);
}

void Light::UpdateCameraConstantBuffer(FrameResource& frameResource, UINT idx)
{
    frameResource.m_cameraConstantBuffers[m_cameraConstantBufferIndex[idx]]->Update(&m_cameraConstantData[idx]);
}

void Light::UpdateLightConstantBuffer(FrameResource& frameResource)
{
    frameResource.m_lightConstantBuffers[m_lightConstantBufferIndex]->Update(&m_lightConstantData);
}

DirectionalLight::DirectionalLight(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    UINT shadowMapResolution,
    ResourceLayoutTracker& layoutTracker,
    const std::vector<std::unique_ptr<FrameResource>>& frameResources,
    DescriptorAllocation&& cbvAllocation)
    : Light(std::move(dsvAllocation), std::move(srvAllocation), LightType::DIRECTIONAL)
{
    Init(pDevice, shadowMapResolution, layoutTracker, frameResources, std::move(cbvAllocation));
}

XMVECTOR DirectionalLight::GetPosition() const
{
    assert(false);
    return XMVectorZero();
}

float DirectionalLight::GetRange() const
{
    assert(false);
    return 0.0f;
}

void DirectionalLight::SetPosition(XMFLOAT3 pos)
{
    assert(false);
}

void DirectionalLight::SetPosition(XMVECTOR pos)
{
    assert(false);
}

void DirectionalLight::SetRange(float range)
{
    assert(false);
}

PointLight::PointLight(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    UINT shadowMapResolution,
    ResourceLayoutTracker& layoutTracker,
    const std::vector<std::unique_ptr<FrameResource>>& frameResources,
    DescriptorAllocation&& cbvAllocation)
    : Light(std::move(dsvAllocation), std::move(srvAllocation), LightType::POINT)
{
    Init(pDevice, shadowMapResolution, layoutTracker, frameResources, std::move(cbvAllocation));
}

XMVECTOR PointLight::GetDirection() const
{
    assert(false);
    return XMVectorZero();
}

void PointLight::SetDirection(XMFLOAT3 dir)
{
    assert(false);
}

void PointLight::SetDirection(XMVECTOR dir)
{
    assert(false);
}

SpotLight::SpotLight(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    UINT shadowMapResolution,
    ResourceLayoutTracker& layoutTracker,
    const std::vector<std::unique_ptr<FrameResource>>& frameResources,
    DescriptorAllocation&& cbvAllocation)
    : Light(std::move(dsvAllocation), std::move(srvAllocation), LightType::SPOT)
{
    Init(pDevice, shadowMapResolution, layoutTracker, frameResources, std::move(cbvAllocation));
}

void SpotLight::SetAngle(float angle)
{
    m_lightConstantData.angle = angle;
}