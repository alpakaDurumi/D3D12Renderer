#pragma once

#include <DirectXMath.h>

using namespace DirectX;

// CRTP for 256-byte alignment
template<typename T>
struct ConstantData
{
    // static_assert must be in in a member function (like the constructor)
    // because the derived class T is an incomplete type at the point of inheritance
    ConstantData()
    {
        static_assert((sizeof(T) % 256) == 0, "Constant Buffer size must be 256-byte aligned");
    }
};

struct SceneConstantData : public ConstantData<SceneConstantData>
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 view;
    XMFLOAT4X4 projection;
    XMFLOAT4X4 inverseTranspose;
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
    float padding2[52];
};

struct CameraConstantData : public ConstantData<CameraConstantData>
{
    XMFLOAT3 cameraPos;
    float padding[61];
};
