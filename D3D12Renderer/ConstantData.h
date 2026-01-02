#pragma once

#include <DirectXMath.h>

using namespace DirectX;

// CRTP for 256-byte alignment
template<typename T>
struct ConstantData
{
    // static_assert must be in a member function (like the constructor)
    // because the derived class T is an incomplete type at the point of inheritance
    ConstantData()
    {
        static_assert((sizeof(T) % 256) == 0, "Constant Buffer size must be 256-byte aligned");
    }
};

struct MeshConstantData : public ConstantData<MeshConstantData>
{
    XMFLOAT4X4 world = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    XMFLOAT4X4 inverseTranspose;
    float textureTileScale = 1.0f;
    float padding[31];
};

struct MaterialConstantData : public ConstantData<MaterialConstantData>
{
    XMFLOAT3 materialAmbient;
    float padding0;
    XMFLOAT3 materialSpecular;
    float shininess;
    float padding1[56];
};

struct LightConstantData : public ConstantData<LightConstantData>
{
    XMFLOAT3 lightPos;
    float padding0;
    XMFLOAT3 lightDir;
    float padding1;
    XMFLOAT3 lightColor;
    float lightIntensity;
    XMFLOAT4X4 viewProjection;
    float padding2[36];
};

struct CameraConstantData : public ConstantData<CameraConstantData>
{
    XMFLOAT3 cameraPos;
    float padding0;
    XMFLOAT4X4 viewProjection;
    float padding1[44];
};