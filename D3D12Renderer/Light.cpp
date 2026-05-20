#include "pch.h"
#include "Light.h"

#include "D3DHelper.h"

using namespace D3DHelper;

Light::Light(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    DescriptorAllocation&& cbvAllocation,
    UINT shadowMapResolution,
    LightType type)
    : m_srv(std::move(srvAllocation)),
    m_lightCbv(std::move(cbvAllocation)),
    m_type(type)
{
    const UINT16 arraySize = GetRequiredArraySize(m_type);

    m_dsvs.resize(arraySize);
    auto dsvAllocs = dsvAllocation.Split();
    for (UINT i = 0; i < arraySize; ++i)
        m_dsvs[i] = DepthStencilView(std::move(dsvAllocs[i]));

    const auto clearValue = CreateClearValue(DXGI_FORMAT_D32_FLOAT, 0.0f, 0);

    // Create depth buffer, init DSVs
    m_depthBuffer = Texture(
        pDevice,
        GetTexture2DDesc(shadowMapResolution, shadowMapResolution, arraySize, 1, DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
        D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
        &clearValue);
    for (UINT i = 0; i < arraySize; ++i)
        m_dsvs[i].Init(pDevice, m_depthBuffer.Get(), GetDsvDesc2DArray(DXGI_FORMAT_D32_FLOAT, i));

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

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetDsvHandle(UINT index) const
{
    return m_dsvs[index].GetHandle();
}

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetSrvHandle() const
{
    return m_srv.GetHandle();
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

D3D12_CPU_DESCRIPTOR_HANDLE Light::GetLightCbvHandle() const
{
    return m_lightCbv.GetHandle();
}

void Light::InitLightCbv(ID3D12Device10* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr)
{
    m_lightCbv.Init(pDevice, gpuPtr, sizeof(LightConstantData));
}

std::vector<GpuResource> Light::TakeResources()
{
    std::vector<GpuResource> ret;
    ret.push_back(std::move(m_depthBuffer));
    return ret;
}

UINT16 Light::GetRequiredArraySize(LightType type)
{
    switch (type)
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

DirectionalLight::DirectionalLight(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    DescriptorAllocation&& cbvAllocation,
    UINT shadowMapResolution)
    : Light(pDevice, std::move(dsvAllocation), std::move(srvAllocation), std::move(cbvAllocation), shadowMapResolution, LightType::DIRECTIONAL)
{
    m_srv.Init(pDevice, m_depthBuffer.Get(), GetSrvDesc2DArray(DXGI_FORMAT_R32_FLOAT, 1, MAX_CASCADES));
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
    : Light(pDevice, std::move(dsvAllocation), std::move(srvAllocation), std::move(cbvAllocation), shadowMapResolution, LightType::POINT)
{
    auto rtvAllocs = rtvAllocation.Split();
    for (UINT i = 0; i < POINT_LIGHT_ARRAY_SIZE; ++i)
        m_rtvs[i] = RenderTargetView(std::move(rtvAllocs[i]));

    auto clearValue = CreateClearValue(DXGI_FORMAT_R32_FLOAT, 1.0f, 0.0f, 0.0f, 0.0f);

    // Create render target, init RTVs
    m_renderTarget = Texture(
        pDevice,
        GetTexture2DDesc(shadowMapResolution, shadowMapResolution, POINT_LIGHT_ARRAY_SIZE, 1, DXGI_FORMAT_R32_TYPELESS, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET),
        D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        &clearValue);
    for (UINT i = 0; i < POINT_LIGHT_ARRAY_SIZE; ++i)
        m_rtvs[i].Init(pDevice, m_renderTarget.Get(), GetRtvDesc2DArray(DXGI_FORMAT_R32_FLOAT, 0, i, 1));

    // Init SRV for render target we've created just before. NOT for depth buffer!
    m_srv.Init(pDevice, m_renderTarget.Get(), GetSrvDescCube(DXGI_FORMAT_R32_FLOAT, 1));
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

D3D12_CPU_DESCRIPTOR_HANDLE PointLight::GetRtvHandle(UINT index) const
{
    return m_rtvs[index].GetHandle();
}

std::vector<GpuResource> PointLight::TakeResources()
{
    auto ret = Light::TakeResources();
    ret.push_back(std::move(m_renderTarget));
    return ret;
}

SpotLight::SpotLight(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    DescriptorAllocation&& cbvAllocation,
    UINT shadowMapResolution)
    : Light(pDevice, std::move(dsvAllocation), std::move(srvAllocation), std::move(cbvAllocation), shadowMapResolution, LightType::SPOT)
{
    m_srv.Init(pDevice, m_depthBuffer.Get(), GetSrvDesc(DXGI_FORMAT_R32_FLOAT, 1));
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