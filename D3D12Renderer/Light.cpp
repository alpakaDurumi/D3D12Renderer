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

    CreateDepthStencilBuffer(pDevice, shadowMapResolution, shadowMapResolution, m_depthBuffer, m_dsvAllocation, arraySize);
    layoutTracker.RegisterResource(m_depthBuffer.Get(), D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, arraySize, 1, DXGI_FORMAT_R32_TYPELESS);

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
    return m_srvAllocation;
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

UINT Light::GetIdxInArray() const
{
    return m_lightConstantData.idxInArray;
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

void Light::SetIdxInArray(UINT idxInArray)
{
    m_lightConstantData.idxInArray = idxInArray;
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

    // Create render target.
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC1 resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = shadowMapResolution;
    resourceDesc.Height = shadowMapResolution;
    resourceDesc.DepthOrArraySize = POINT_LIGHT_ARRAY_SIZE;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    resourceDesc.SampleDesc = { 1, 0 };
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    resourceDesc.SamplerFeedbackMipRegion = {};     // Not use Sampler Feedback

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R32_FLOAT;
    clearValue.Color[0] = 1.0f;
    clearValue.Color[1] = 1.0f;
    clearValue.Color[2] = 1.0f;
    clearValue.Color[3] = 1.0f;

    ThrowIfFailed(pDevice->CreateCommittedResource3(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        &clearValue,
        nullptr,
        0,
        nullptr,
        IID_PPV_ARGS(&m_renderTarget)));

    // Create RTVs.
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Texture2DArray.MipSlice = 0;
    rtvDesc.Texture2DArray.ArraySize = 1;
    rtvDesc.Texture2DArray.PlaneSlice = 0;
    for (UINT i = 0; i < POINT_LIGHT_ARRAY_SIZE; ++i)
    {
        rtvDesc.Texture2DArray.FirstArraySlice = i;
        pDevice->CreateRenderTargetView(m_renderTarget.Get(), &rtvDesc, m_rtvAllocation.GetDescriptorHandle(i));
    }

    layoutTracker.RegisterResource(m_renderTarget.Get(), D3D12_BARRIER_LAYOUT_RENDER_TARGET, POINT_LIGHT_ARRAY_SIZE, 1, DXGI_FORMAT_R32_TYPELESS);

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
}

void SpotLight::SetAngle(float angle)
{
    m_lightConstantData.angle = angle;
}