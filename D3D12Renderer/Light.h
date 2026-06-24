#pragma once

#include <array>
#include <unordered_map>
#include <vector>

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <basetsd.h>
#include <d3d12.h>
#include <minwindef.h>

#include "ConstantData.h"
#include "SceneHandles.h"
#include "SharedConfig.h"
#include "Texture.h"
#include "UploadAllocation.h"
#include "View.h"
#include "VisibleRange.h"

class DescriptorAllocation;
class GpuResource;

class Light
{
protected:
    Light(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        UINT shadowMapResolution,
        LightType type);

public:
    LightType GetType() const;
    ID3D12Resource* GetDepthBuffer() const;
    UINT16 GetArraySize() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetSrvHandle() const;

    virtual DirectX::XMVECTOR GetPosition() const;
    virtual DirectX::XMVECTOR GetDirection() const;
    virtual float GetRange() const;

    UINT GetIdxInArray() const;

    virtual void SetPosition(DirectX::XMFLOAT3 pos);
    virtual void SetPosition(DirectX::XMVECTOR pos);
    virtual void SetDirection(DirectX::XMFLOAT3 dir);
    virtual void SetDirection(DirectX::XMVECTOR dir);
    virtual void SetRange(float range);

    virtual void SetViewProjection(DirectX::XMMATRIX view, DirectX::XMMATRIX projection, UINT idx);

    void SetIdxInArray(UINT idxInArray);

    CameraConstantData* GetCameraConstantDataPtr(UINT arrayIndex);
    void SetCameraUploadAllocation(UINT arrayIndex, UploadAllocation alloc);
    UploadAllocation GetCameraUploadAllocation(UINT arrayIndex);

    LightConstantData* GetLightConstantDataPtr();
    D3D12_CPU_DESCRIPTOR_HANDLE GetLightCbvHandle() const;
    void InitLightCbv(ID3D12Device* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr);

    const VisibleRange& GetVisibleRange(MeshHandle meshHandle, UINT arrayIndex = 0) const;
    void SetVisibleRange(MeshHandle meshHandle, VisibleRange visibleRange, UINT arrayIndex = 0);
    void ResetVisibleRange();

    virtual std::vector<GpuResource> TakeResources();

protected:
    std::vector<CameraConstantData> m_cameraConstantData;
    std::vector<UploadAllocation> m_cameraUploadAllocations; // transient, for single frame

    LightConstantData m_lightConstantData;
    ConstantBufferView m_lightCbv;

    LightType m_type;

    Texture m_depthBuffer;
    std::vector<DepthStencilView> m_dsvs;
    ShaderResourceView m_srv;

    std::vector<std::unordered_map<MeshHandle, VisibleRange>> m_visibleRanges;
};

class DirectionalLight : public Light
{
public:
    DirectionalLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        UINT shadowMapResolution);

    virtual DirectX::XMVECTOR GetPosition() const override;
    virtual float GetRange() const override;

    void SetPosition(DirectX::XMFLOAT3 pos) override;
    void SetPosition(DirectX::XMVECTOR pos) override;

    virtual void SetRange(float range) override;

    const std::array<DirectX::BoundingOrientedBox, MAX_CASCADES>& GetBoundingBoxes() const;
    void SetBoundingBox(UINT arrayIndex, const DirectX::BoundingOrientedBox& boundingBox);

private:
    std::array<DirectX::BoundingOrientedBox, MAX_CASCADES> m_boundingBoxes;
};

class PointLight : public Light
{
public:
    PointLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        DescriptorAllocation&& rtvAllocation,
        UINT shadowMapResolution);

    DirectX::XMVECTOR GetDirection() const override;

    void SetDirection(DirectX::XMFLOAT3 dir) override;
    void SetDirection(DirectX::XMVECTOR dir) override;

    void SetViewProjection(DirectX::XMMATRIX view, DirectX::XMMATRIX projection, UINT idx) override;

    ID3D12Resource* GetRenderTarget() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(UINT index) const;

    const DirectX::BoundingSphere& GetBoundingSphere() const;
    void SetBoundingSphere(const DirectX::BoundingSphere& boundingSphere);

    virtual std::vector<GpuResource> TakeResources() override;

private:
    Texture m_renderTarget;
    std::array<RenderTargetView, POINT_LIGHT_ARRAY_SIZE> m_rtvs;

    DirectX::BoundingSphere m_boundingSphere;
};

class SpotLight : public Light
{
public:
    SpotLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        UINT shadowMapResolution);

    float GetOuterAngle() const;
    void SetAngles(float outerAngle, float innerAngle);

    const DirectX::BoundingFrustum& GetBoundingFrustum() const;
    void SetBoundingFrustum(const DirectX::BoundingFrustum& boundingFrustum);

private:
    float m_outerAngle;
    float m_innerAngle;

    DirectX::BoundingFrustum m_boundingFrustum;
};
