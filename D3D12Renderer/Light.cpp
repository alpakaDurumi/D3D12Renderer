#include "pch.h"

#include "Light.h"

#include "D3DHelper.h"
#include "DescriptorAllocation.h"
#include "GpuResource.h"

using namespace DirectX;
using namespace D3DHelper;

static UINT16 GetRequiredArraySize(LightType type)
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

static UINT GetBoundingVolumeCount(LightType type)
{
    switch (type)
    {
    case LightType::DIRECTIONAL:
        return MAX_CASCADES;
    case LightType::POINT:
    case LightType::SPOT:
        return 1;
    default:
        return -1;
    }
}

Light::Light(
    ID3D12Device10* pDevice,
    DescriptorAllocation&& dsvAllocation,
    DescriptorAllocation&& srvAllocation,
    DescriptorAllocation&& cbvAllocation,
    UINT shadowMapResolution,
    LightType type)
    : m_lightCbv(std::move(cbvAllocation))
    , m_type(type)
    , m_srv(std::move(srvAllocation))
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

    m_visibleRanges.resize(GetBoundingVolumeCount(m_type));
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

UINT Light::GetIdxInArray() const
{
    return m_lightConstantData.idxInArray;
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

void Light::InitLightCbv(ID3D12Device* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr)
{
    m_lightCbv.Init(pDevice, gpuPtr, sizeof(LightConstantData));
}

const VisibleRange& Light::GetVisibleRange(MeshHandle meshHandle, UINT arrayIndex) const
{
    const auto& umap = m_visibleRanges[arrayIndex];

    auto it = umap.find(meshHandle);
    assert(it != umap.end());

    return it->second;
}

void Light::SetVisibleRange(MeshHandle meshHandle, VisibleRange visibleRange, UINT arrayIndex)
{
    m_visibleRanges[arrayIndex][meshHandle] = visibleRange;
}

void Light::ResetVisibleRange()
{
    for (auto& umap : m_visibleRanges)
        umap.clear();
}

std::vector<GpuResource> Light::TakeResources()
{
    std::vector<GpuResource> ret;
    ret.push_back(std::move(m_depthBuffer));
    return ret;
}

void Light::SetPositionConstants(XMVECTOR pos)
{
    for (auto& cd : m_cameraConstantData)
        cd.SetPos(pos);

    m_lightConstantData.SetPos(pos);
}

void Light::SetDirectionConstants(XMVECTOR dir)
{
    m_lightConstantData.SetLightDir(dir);
}

void Light::SetRangeConstants(float range)
{
    for (auto& cd : m_cameraConstantData)
        cd.farPlane = range;
    m_lightConstantData.range = range;
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

void DirectionalLight::SetWorldTransform(XMMATRIX world)
{
    SetDirectionConstants(XMVector3Normalize(world.r[2]));
}

void DirectionalLight::SetShadowContext(XMVECTOR cameraPos, float cameraFar, UINT shadowMapResolution, const std::vector<BoundingSphere>& cascadeSpheres)
{
    static XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR dir = XMLoadFloat3(&m_lightConstantData.lightDir);

    for (UINT i = 0; i < MAX_CASCADES; ++i)
    {
        XMVECTOR center = XMLoadFloat3(&cascadeSpheres[i].Center);
        float radius = cascadeSpheres[i].Radius;

        XMVECTOR viewOriginToCenter = center - cameraPos;

        // Calculate view/projection matrix fit to light frustum

        // Orthogonal projection of (center - view origin) onto lightDir.
        // This represents where the view origin is located relative to the center on the light's Z-axis.
        float d = XMVectorGetX(XMVector3Dot(viewOriginToCenter, dir));

        XMMATRIX view = XMMatrixLookToLH(center, dir, up);
        // Near Plane : Set to (view origin - sceneRadius) in Light Space.
        //              This ensures all shadow casters within 'sceneRadius' behind the camera are captured.
        // Far Plane :  Set to 'radius' to cover the entire bounding sphere of the view frustum.
        // Argument for NearZ and FarZ are swapped because of reverse-z
        float nearZ = -d - cameraFar;
        float farZ = radius;
        XMMATRIX projection = XMMatrixOrthographicLH(2 * radius, 2 * radius, farZ, nearZ);

        // Apply texel-sized increments to eliminate shadow shimmering.
        XMVECTOR shadowOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin = XMVector4Transform(shadowOrigin, view * projection);
        shadowOrigin = XMVectorScale(shadowOrigin, 1.0f / XMVectorGetW(shadowOrigin)); // Perspective divide. Can be ommitted if it uses orthographic projection.
        // [-1, 1] -> [-resolution / 2, resolution / 2]
        shadowOrigin = XMVectorScale(shadowOrigin, static_cast<float>(shadowMapResolution) * 0.5f); // Scaling based on shadow map resolution. We only need to scale it. No need to offset.

        // Calculate diff and apply as translation matrix.
        XMVECTOR roundedOrigin = XMVectorRound(shadowOrigin);
        XMVECTOR diff = roundedOrigin - shadowOrigin;
        diff = XMVectorScale(diff, 2.0f / static_cast<float>(shadowMapResolution)); // Since diff is texel scale, it should be transformed to NDC scale.
        XMMATRIX fix = XMMatrixTranslation(XMVectorGetX(diff), XMVectorGetY(diff), 0.0f);

        SetViewProjection(view, projection * fix, i);

        // Set bounding box
        BoundingOrientedBox boundingBox(
            {0.0f, 0.0f, (nearZ + farZ) * 0.5f},
            {radius, radius, (farZ - nearZ) * 0.5f},
            {0.0f, 0.0f, 0.0f, 1.0f});
        boundingBox.Transform(m_boundingBoxes[i], XMMatrixInverse(nullptr, view));
    }
}

const std::array<BoundingOrientedBox, MAX_CASCADES>& DirectionalLight::GetBoundingBoxes() const
{
    return m_boundingBoxes;
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

void PointLight::SetWorldTransform(XMMATRIX world)
{
    SetPositionConstants(world.r[3]);
}

void PointLight::SetShadowContext(float cameraNear)
{
    XMVECTOR pos = XMVectorSetW(XMLoadFloat3(&m_lightConstantData.lightPos), 1.0f);

    // +X, -X, +Y, -Y, +Z, -Z
    static const XMVECTOR Directions[6] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, -1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f}};
    static const XMVECTOR Ups[6] = {
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f}};

    // Set FOV as 90 degree
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, m_lightConstantData.range, cameraNear);

    for (UINT i = 0; i < POINT_LIGHT_ARRAY_SIZE; ++i)
    {
        XMMATRIX view = XMMatrixLookToLH(pos, Directions[i], Ups[i]);
        SetViewProjection(view, projection, i);
    }

    // Set bounding sphere
    m_boundingSphere = BoundingSphere(m_lightConstantData.lightPos, m_lightConstantData.range);
}

void PointLight::SetViewProjection(XMMATRIX view, XMMATRIX projection, UINT idx)
{
    m_cameraConstantData[idx].SetView(view);
    m_cameraConstantData[idx].SetProjection(projection);
}

ID3D12Resource* PointLight::GetRenderTarget() const
{
    return m_renderTarget.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE PointLight::GetRtvHandle(UINT index) const
{
    return m_rtvs[index].GetHandle();
}

const BoundingSphere& PointLight::GetBoundingSphere() const
{
    return m_boundingSphere;
}

std::vector<GpuResource> PointLight::TakeResources()
{
    auto ret = Light::TakeResources();
    ret.push_back(std::move(m_renderTarget));
    return ret;
}

float PointLight::GetRange() const
{
    return m_lightConstantData.range;
}

void PointLight::SetRange(float range)
{
    SetRangeConstants(range);
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
    SetAngles(45.0f, 20.0f); // Set default angle
}

void SpotLight::SetWorldTransform(XMMATRIX world)
{
    SetPositionConstants(world.r[3]);
    SetDirectionConstants(XMVector3Normalize(world.r[2]));
}

void SpotLight::SetShadowContext(float cameraNear)
{
    static XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR pos = XMVectorSetW(XMLoadFloat3(&m_lightConstantData.lightPos), 1.0f);
    XMVECTOR dir = XMLoadFloat3(&m_lightConstantData.lightDir);

    XMMATRIX view = XMMatrixLookToLH(pos, dir, up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(m_outerAngle, 1.0f, m_lightConstantData.range, cameraNear);
    SetViewProjection(view, projection, 0);

    // Set bounding frustum
    XMMATRIX proj = XMMatrixPerspectiveFovLH(m_outerAngle, 1.0f, cameraNear, m_lightConstantData.range);
    BoundingFrustum::CreateFromMatrix(m_boundingFrustum, proj);
    m_boundingFrustum.Transform(m_boundingFrustum, XMMatrixInverse(nullptr, view));
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

const BoundingFrustum& SpotLight::GetBoundingFrustum() const
{
    return m_boundingFrustum;
}

float SpotLight::GetRange() const
{
    return m_lightConstantData.range;
}

void SpotLight::SetRange(float range)
{
    SetRangeConstants(range);
}
