#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct SceneConstantData
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 view;
    XMFLOAT4X4 projection;
    XMFLOAT4X4 inverseTranspose;
};
static_assert((sizeof(SceneConstantData) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct MaterialConstantData
{
    XMFLOAT3 materialAmbient;
    float padding0;
    XMFLOAT3 materialSpecular;
    float shininess;
    float padding[56];
};
static_assert((sizeof(MaterialConstantData) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct LightConstantData
{
    XMFLOAT3 lightPos;
    float padding0;
    XMFLOAT3 lightDir;
    float padding1;
    XMFLOAT3 lightColor;
    float lightIntensity;
    float padding[52];
};
static_assert((sizeof(LightConstantData) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

struct CameraConstantData
{
    XMFLOAT3 cameraPos;
    float padding[61];
};
static_assert((sizeof(CameraConstantData) % 256) == 0, "Constant Buffer size must be 256-byte aligned");
