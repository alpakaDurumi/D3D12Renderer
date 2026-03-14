#include "pch.h"
#include "Light.h"

#include "D3DHelper.h"
#include "SharedConfig.h"

using namespace D3DHelper;

Light::Light(DescriptorAllocation&& dsvAllocation, DescriptorAllocation&& srvAllocation, LightType type)
    : m_dsvAllocation(std::move(dsvAllocation)),
    m_srvAllocation(std::move(srvAllocation)),
    m_type(type)
{
    UINT16 arraySize = GetRequiredArraySize(type);

    assert(m_dsvAllocation.GetNumHandles() == arraySize && !m_srvAllocation.IsNull());

    m_lightConstantData.type = static_cast<UINT32>(m_type);
    m_cameraConstantData.resize(arraySize);
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

    CreateDepthStencilBuffer(pDevice, shadowMapResolution, shadowMapResolution, arraySize, m_depthBuffer);
    for (UINT i = 0; i < arraySize; ++i)
    {
        CreateDSV(pDevice, m_depthBuffer.Get(), m_dsvAllocation.GetDescriptorHandle(i), true, i);
    }
    layoutTracker.RegisterResource(m_depthBuffer.Get(), D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, arraySize, 1, DXGI_FORMAT_R32_TYPELESS);

    auto cbvAllocations = cbvAllocation.Split();

    // Create constant buffers
    for (UINT i = 0; i < frameResources.size(); ++i)
    {
        FrameResource& frameResource = *frameResources[i];

        frameResource.AddLightConstantBuffer(std::move(cbvAllocations[i]));
        if (i == 0) m_cameraConstantBufferBaseIndex = frameResource.GetCameraConstantBufferCount();

        for (UINT j = 0; j < arraySize; ++j)
        {
            frameResource.AddCameraConstantBuffer();
        }
    }
}

LightType Light::GetType() const
{
    return m_type;
}

ID3D12Resource* Light::GetDepthBuffer() const
{
    return m_depthBuffer.Get();
}

UINT16 Light::GetArraySize() const
{
    return GetRequiredArraySize(m_type);
}

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetDSVDescriptorHandle(UINT idx) const
{
    return m_dsvAllocation.GetDescriptorHandle(idx);
}

UINT Light::GetCameraConstantBufferBaseIndex() const
{
    return m_cameraConstantBufferBaseIndex;
}
//
//UINT Light::GetLightConstantBufferIndex() const
//{
//    return m_lightConstantBufferIndex;
//}

DescriptorAllocation& Light::GetSRVAllocationRef()
{
    return m_srvAllocation;
}

XMVECTOR Light::GetPosition() const
{
    XMVECTOR p = XMLoadFloat3(&m_lightConstantData.lightPos);
    return XMVectorSetW(p, 1.0f);
}

XMVECTOR Light::GetDirection() const
{
    XMVECTOR d = XMLoadFloat3(&m_lightConstantData.lightDir);
    return XMVectorSetW(d, 0.0f);
}

float Light::GetRange() const
{
    return m_lightConstantData.range;
}

UINT Light::GetIdxInArray() const
{
    return m_lightConstantData.idxInArray;
}

void Light::SetPosition(XMFLOAT3 pos)
{
    m_lightConstantData.lightPos = pos;
}

void Light::SetPosition(XMVECTOR pos)
{
    m_lightConstantData.SetPos(pos);
}

void Light::SetDirection(XMFLOAT3 dir)
{
    m_lightConstantData.lightDir = dir;
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

void Light::SetIdxInArray(UINT idxInArray)
{
    m_lightConstantData.idxInArray = idxInArray;
}

CameraConstantData* Light::GetCameraConstantDataPtr(UINT idx)
{
    return &m_cameraConstantData[idx];
}

LightConstantData* Light::GetLightConstantDataPtr()
{
    return &m_lightConstantData;
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
    CreateSRVForShadow(pDevice, m_depthBuffer.Get(), m_srvAllocation.GetDescriptorHandle(), m_type);
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
    DescriptorAllocation&& cbvAllocation,
    DescriptorAllocation&& rtvAllocation)
    : Light(std::move(dsvAllocation), std::move(srvAllocation), LightType::POINT)
    , m_rtvAllocation(std::move(rtvAllocation))
{
    assert(POINT_LIGHT_ARRAY_SIZE == m_rtvAllocation.GetNumHandles());

    Init(pDevice, shadowMapResolution, layoutTracker, frameResources, std::move(cbvAllocation));

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R32_FLOAT;
    clearValue.Color[0] = 1.0f;
    clearValue.Color[1] = 1.0f;
    clearValue.Color[2] = 1.0f;
    clearValue.Color[3] = 1.0f;

    CreateRenderTarget(pDevice, shadowMapResolution, shadowMapResolution, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, POINT_LIGHT_ARRAY_SIZE, m_renderTarget, &clearValue);
    layoutTracker.RegisterResource(m_renderTarget.Get(), D3D12_BARRIER_LAYOUT_RENDER_TARGET, POINT_LIGHT_ARRAY_SIZE, 1, DXGI_FORMAT_R32_TYPELESS);

    // Create RTVs.
    for (UINT i = 0; i < POINT_LIGHT_ARRAY_SIZE; ++i)
    {
        CreateRTV(pDevice, m_renderTarget.Get(), DXGI_FORMAT_R32_FLOAT, m_rtvAllocation.GetDescriptorHandle(i), true, i);
    }

    // Create SRV for render target we've created just before. NOT for depth buffer!
    CreateSRVForShadow(pDevice, m_renderTarget.Get(), m_srvAllocation.GetDescriptorHandle(), m_type);
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

ID3D12Resource* PointLight::GetRenderTarget() const
{
    return m_renderTarget.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE PointLight::GetRTVDescriptorHandle(UINT idx) const
{
    return m_rtvAllocation.GetDescriptorHandle(idx);
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
    CreateSRVForShadow(pDevice, m_depthBuffer.Get(), m_srvAllocation.GetDescriptorHandle(), m_type);
}

float SpotLight::GetOuterAngle() const
{
    return m_outerAngle;
}

void SpotLight::SetAngles(float outerAngleDegree, float innerAngleDegree)
{
    assert(outerAngleDegree >= innerAngleDegree);

    m_outerAngle = XMConvertToRadians(outerAngleDegree);
    m_innerAngle = XMConvertToRadians(innerAngleDegree);
    // We need cosine value that calculated from half angle.
    m_lightConstantData.cosOuterAngle = std::cos(m_outerAngle * 0.5f);
    m_lightConstantData.cosInnerAngle = std::cos(m_innerAngle * 0.5f);
}