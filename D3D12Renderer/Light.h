#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include "ConstantData.h"
#include "DescriptorAllocation.h"
#include "ResourceLayoutTracker.h"
#include "FrameResource.h"
#include "SharedConfig.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class Light
{
protected:
    Light(DescriptorAllocation&& dsvAllocation, DescriptorAllocation&& srvAllocation, LightType type);

    void Init(
        ID3D12Device10* pDevice,
        UINT shadowMapResolution,
        ResourceLayoutTracker& layoutTracker,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources,
        DescriptorAllocation&& cbvAllocation);

public:
    LightType GetType() const;
    ID3D12Resource* GetDepthBuffer() const;
    UINT16 GetArraySize() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVDescriptorHandle(UINT idx) const;

    UINT GetCameraConstantBufferIndex(UINT idx) const;
    UINT GetLightConstantBufferIndex() const;

    DescriptorAllocation& GetSRVAllocationRef();

    virtual XMVECTOR GetPosition() const;
    virtual XMVECTOR GetDirection() const;
    virtual float GetRange() const;

    UINT GetIdxInArray() const;

    virtual void SetPosition(XMFLOAT3 pos);
    virtual void SetPosition(XMVECTOR pos);
    virtual void SetDirection(XMFLOAT3 dir);
    virtual void SetDirection(XMVECTOR dir);
    virtual void SetRange(float range);

    void SetViewProjection(XMMATRIX view, XMMATRIX projection, UINT idx);

    void SetIdxInArray(UINT idxInArray);

    void UpdateCameraConstantBuffer(FrameResource& frameResource, UINT idx);
    void UpdateLightConstantBuffer(FrameResource& frameResource);

protected:
    std::vector<CameraConstantData> m_cameraConstantData;
    std::vector<UINT> m_cameraConstantBufferIndex;

    LightConstantData m_lightConstantData;
    UINT m_lightConstantBufferIndex;

    LightType m_type;

    ComPtr<ID3D12Resource> m_depthBuffer;
    DescriptorAllocation m_dsvAllocation;
    DescriptorAllocation m_srvAllocation;
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
        DescriptorAllocation&& cbvAllocation);

    virtual XMVECTOR GetPosition() const override;
    virtual float GetRange() const override;

    void SetPosition(XMFLOAT3 pos) override;
    void SetPosition(XMVECTOR pos) override;

    virtual void SetRange(float range) override;
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
        DescriptorAllocation&& cbvAllocation,
        DescriptorAllocation&& rtvAllocation);

    XMVECTOR GetDirection() const override;

    void SetDirection(XMFLOAT3 dir) override;
    void SetDirection(XMVECTOR dir) override;

    ID3D12Resource* GetRenderTarget() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVDescriptorHandle(UINT idx) const;

private:
    ComPtr<ID3D12Resource> m_renderTarget;
    DescriptorAllocation m_rtvAllocation;
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
        DescriptorAllocation&& cbvAllocation);

    void SetAngle(float angle);
};