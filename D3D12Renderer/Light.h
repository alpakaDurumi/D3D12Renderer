#pragma once

#include <minwindef.h>
#include <basetsd.h>

#include <d3d12.h>
#include <DirectXMath.h>

#include <vector>

#include "ConstantData.h"
#include "DescriptorAllocation.h"
#include "SharedConfig.h"
#include "UploadAllocation.h"
#include "Texture.h"
#include "View.h"

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

    void SetViewProjection(DirectX::XMMATRIX view, DirectX::XMMATRIX projection, UINT idx);

    void SetIdxInArray(UINT idxInArray);

    CameraConstantData* GetCameraConstantDataPtr(UINT arrayIndex);
    void SetCameraUploadAllocation(UINT arrayIndex, UploadAllocation alloc);
    UploadAllocation GetCameraUploadAllocation(UINT arrayIndex);

    LightConstantData* GetLightConstantDataPtr();
    D3D12_CPU_DESCRIPTOR_HANDLE GetLightCbvHandle() const;
    void InitLightCbv(ID3D12Device10* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr);

    virtual std::vector<GpuResource> TakeResources();

    static UINT16 GetRequiredArraySize(LightType type);

protected:
    std::vector<CameraConstantData> m_cameraConstantData;
    std::vector<UploadAllocation> m_cameraUploadAllocations;    // transient, for single frame

    LightConstantData m_lightConstantData;
    ConstantBufferView m_lightCbv;

    LightType m_type;

    Texture m_depthBuffer;
    std::vector<DepthStencilView> m_dsvs;
    ShaderResourceView m_srv;
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

    ID3D12Resource* GetRenderTarget() const;
    D3D12_CPU_DESCRIPTOR_HANDLE GetRtvHandle(UINT index) const;

    virtual std::vector<GpuResource> TakeResources() override;

private:
    Texture m_renderTarget;
    std::array<RenderTargetView, POINT_LIGHT_ARRAY_SIZE> m_rtvs;
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

private:
    float m_outerAngle;
    float m_innerAngle;
};