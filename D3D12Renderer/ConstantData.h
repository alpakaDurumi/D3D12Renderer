#pragma once

#include <Windows.h>

#include <d3d12.h>
#include <DirectXMath.h>

#include "SharedConfig.h"

// CRTP for 256-byte alignment
template<typename T>
struct ConstantData
{
    // static_assert must be in a member function (like the constructor)
    // because the derived class T is an incomplete type at the point of inheritance
    ConstantData()
    {
        static_assert((sizeof(T) % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) == 0, "Constant Buffer size must be 256-byte aligned");
    }
};

struct CameraConstantData : public ConstantData<CameraConstantData>
{
    DirectX::XMFLOAT3 cameraPos;
    float padding0;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
    DirectX::XMFLOAT4X4 invView;
    DirectX::XMFLOAT4X4 invProj;
    float padding1[60];

    void SetPos(DirectX::XMVECTOR pos);
    void SetView(DirectX::XMMATRIX view);
    void SetProjection(DirectX::XMMATRIX projection);
};

struct LightConstantData : public ConstantData<LightConstantData>
{
    DirectX::XMFLOAT3 lightPos = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    float range = 50.0f;
    DirectX::XMFLOAT3 lightDir = DirectX::XMFLOAT3(0.0f, 0.0f, 1.0f);
    float cosOuterAngle;
    DirectX::XMFLOAT3 lightColor = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
    float cosInnerAngle;
    DirectX::XMFLOAT4X4 viewProjection[POINT_LIGHT_ARRAY_SIZE];
    UINT type;
    UINT idxInArray;
    float lightIntensity = 1.0f;
    float padding[17];

    void SetPos(DirectX::XMVECTOR pos);
    void SetLightDir(DirectX::XMVECTOR lightDir);
    void SetViewProjection(DirectX::XMMATRIX viewProjection, UINT idx);
};

struct MaterialConstantData : public ConstantData<MaterialConstantData>
{
    DirectX::XMFLOAT3 materialAmbient;
    float padding0;
    DirectX::XMFLOAT3 materialSpecular;
    float shininess;
    UINT textureIndices[4];
    UINT samplerIndices[4];
    float textureTileScales[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float padding1[44];

    void SetAmbient(DirectX::XMFLOAT4 ambient);
    void SetSpecular(DirectX::XMFLOAT4 specular);
};

struct ShadowConstantData : public ConstantData<ShadowConstantData>
{
    // Each cascade split should be stored in x component. Use XMFLOAT4 because of HLSL alignment rule.
    DirectX::XMFLOAT4 cascadeSplits[MAX_CASCADES];
    float padding[48];
};