#include "pch.h"
#include "Light.h"

#include "D3DHelper.h"
#include "SharedConfig.h"

using namespace D3DHelper;

Light::Light(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    DescriptorAllocation&& cbvAllocation,
    UINT shadowMapResolution,
    LightType type)
    : m_dsvAllocation(std::move(dsvAllocation)),
    m_srvAllocation(std::move(srvAllocation)),
    m_lightCBVAllocations(cbvAllocation.Split()),
    m_type(type)
{
    const UINT16 arraySize = GetRequiredArraySize(m_type);
    assert(m_dsvAllocation.GetNumHandles() == arraySize && !m_srvAllocation.IsNull());

    CreateDepthStencilBuffer(pDevice, shadowMapResolution, shadowMapResolution, arraySize, m_depthBuffer, false);
    for (UINT i = 0; i < arraySize; ++i)
    {
        CreateDSV(pDevice, m_depthBuffer.Get(), m_dsvAllocation.GetDescriptorHandle(i), false, false, true, i);
    }

    m_cameraConstantData.resize(arraySize);
    m_cameraUploadAllocations.resize(arraySize);
    m_lightConstantData.type = static_cast<UINT32>(m_type);
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

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetDSVHandle(UINT idx) const
{
    return m_dsvAllocation.GetDescriptorHandle(idx);
}

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
    SetDirection(XMVectorSet(dir.x, dir.y, dir.z, 0.0f));
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

CameraConstantData* Light::GetCameraConstantDataPtr(UINT arrayIndex)
{
    return &m_cameraConstantData[arrayIndex];
}

void Light::SetCameraUploadAllocation(UINT arrayIndex, UploadAllocation alloc)
{
    m_cameraUploadAllocations[arrayIndex] = alloc;
}

UploadAllocation Light::GetCameraUploadAllocation(UINT arrayIndex)
{
    return m_cameraUploadAllocations[arrayIndex];
}

LightConstantData* Light::GetLightConstantDataPtr()
{
    return &m_lightConstantData;
}

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetLightCBVHandle(UINT frameIndex) const
{
    return m_lightCBVAllocations[frameIndex].GetDescriptorHandle();
}

DescriptorAllocation& Light::GetLightCBVAllocationRef(UINT frameIndex)
{
    return m_lightCBVAllocations[frameIndex];
}

UINT Light::GetDepthBufferHandle() const
{
    return m_hDepthBuffer;
}

void Light::SetDepthBufferHandle(UINT handle)
{
    m_hDepthBuffer = handle;
}

DirectionalLight::DirectionalLight(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    DescriptorAllocation&& cbvAllocation,
    UINT shadowMapResolution)
    : Light(pDevice, std::move(dsvAllocation), std::move(srvAllocation), std::move(cbvAllocation), shadowMapResolution, LightType::DIRECTIONAL)
{
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
    DescriptorAllocation&& cbvAllocation,
    DescriptorAllocation&& rtvAllocation,
    UINT shadowMapResolution)
    : Light(pDevice, std::move(dsvAllocation), std::move(srvAllocation), std::move(cbvAllocation), shadowMapResolution, LightType::POINT),
    m_rtvAllocation(std::move(rtvAllocation))
{
    assert(POINT_LIGHT_ARRAY_SIZE == m_rtvAllocation.GetNumHandles());

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R32_FLOAT;
    clearValue.Color[0] = 1.0f;
    clearValue.Color[1] = 1.0f;
    clearValue.Color[2] = 1.0f;
    clearValue.Color[3] = 1.0f;

    CreateRenderTarget(pDevice, shadowMapResolution, shadowMapResolution, DXGI_FORMAT_R32_TYPELESS, DXGI_FORMAT_R32_FLOAT, POINT_LIGHT_ARRAY_SIZE, m_renderTarget, &clearValue);

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

D3D12_CPU_DESCRIPTOR_HANDLE PointLight::GetRTVHandle(UINT idx) const
{
    return m_rtvAllocation.GetDescriptorHandle(idx);
}

UINT PointLight::GetRenderTargetHandle() const
{
    return m_hRenderTarget;
}

void PointLight::SetRenderTargetHandle(UINT handle)
{
    m_hRenderTarget = handle;
}

SpotLight::SpotLight(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    DescriptorAllocation&& cbvAllocation,
    UINT shadowMapResolution)
    : Light(pDevice, std::move(dsvAllocation), std::move(srvAllocation), std::move(cbvAllocation), shadowMapResolution, LightType::SPOT)
{
    CreateSRVForShadow(pDevice, m_depthBuffer.Get(), m_srvAllocation.GetDescriptorHandle(), m_type);
    SetAngles(45.0f, 20.0f);    // Set default angle
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

    const float minDiff = 0.01f;

    if ((m_lightConstantData.cosInnerAngle - m_lightConstantData.cosOuterAngle) < minDiff)
        m_lightConstantData.cosOuterAngle = m_lightConstantData.cosInnerAngle - minDiff;
}