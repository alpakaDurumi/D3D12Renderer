#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <DirectXMath.h>

#include <vector>

#include "ConstantData.h"
#include "DescriptorAllocation.h"
#include "SharedConfig.h"
#include "UploadAllocation.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;

class Light
{
protected:
    Light(DescriptorAllocation&& dsvAllocation, DescriptorAllocation&& srvAllocation, LightType type);

    void Init(
        ID3D12Device10* pDevice,
        UINT shadowMapResolution,
        DescriptorAllocation&& cbvAllocation);

public:
    LightType GetType() const;
    ID3D12Resource* GetDepthBuffer() const;
    UINT16 GetArraySize() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(UINT idx) const;

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

    CameraConstantData* GetCameraConstantDataPtr(UINT arrayIndex);
    void SetCameraUploadAllocation(UINT arrayIndex, UploadAllocation alloc);
    UploadAllocation GetCameraUploadAllocation(UINT arrayIndex);

    LightConstantData* GetLightConstantDataPtr();
    D3D12_CPU_DESCRIPTOR_HANDLE GetLightCBVHandle(UINT frameIndex) const;
    DescriptorAllocation& GetLightCBVAllocationRef(UINT frameIndex);

    UINT GetDepthBufferHandle() const;
    void SetDepthBufferHandle(UINT handle);

protected:
    std::vector<CameraConstantData> m_cameraConstantData;
    std::vector<UploadAllocation> m_cameraUploadAllocations;    // transient, for single frame

    LightConstantData m_lightConstantData;
    std::vector<DescriptorAllocation> m_lightCBVAllocations;    // for each frame

    LightType m_type;

    ComPtr<ID3D12Resource> m_depthBuffer;
    DescriptorAllocation m_dsvAllocation;
    DescriptorAllocation m_srvAllocation;

    UINT m_hDepthBuffer;
};

class DirectionalLight : public Light
{
public:
    DirectionalLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        UINT shadowMapResolution,
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
        DescriptorAllocation&& cbvAllocation,
        DescriptorAllocation&& rtvAllocation);

    XMVECTOR GetDirection() const override;

    void SetDirection(XMFLOAT3 dir) override;
    void SetDirection(XMVECTOR dir) override;

    ID3D12Resource* GetRenderTarget() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(UINT idx) const;

    UINT GetRenderTargetHandle() const;
    void SetRenderTargetHandle(UINT handle);

private:
    ComPtr<ID3D12Resource> m_renderTarget;
    DescriptorAllocation m_rtvAllocation;

    UINT m_hRenderTarget;
};

class SpotLight : public Light
{
public:
    SpotLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        UINT shadowMapResolution,
        DescriptorAllocation&& cbvAllocation);

    float GetOuterAngle() const;
    void SetAngles(float outerAngle, float innerAngle);

private:
    float m_outerAngle;
    float m_innerAngle;
};