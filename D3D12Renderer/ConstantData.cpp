#include "pch.h"
#include "ConstantData.h"

using namespace DirectX;

// CameraConstantData
void CameraConstantData::SetPos(XMVECTOR pos)
{
    XMStoreFloat3(&this->cameraPos, pos);
}

void CameraConstantData::SetView(XMMATRIX view)
{
    XMStoreFloat4x4(&this->view, XMMatrixTranspose(view));
    XMStoreFloat4x4(&this->invView, XMMatrixTranspose(XMMatrixInverse(nullptr, view)));
}

void CameraConstantData::SetProjection(XMMATRIX projection)
{
    XMStoreFloat4x4(&this->projection, XMMatrixTranspose(projection));
    XMStoreFloat4x4(&this->invProj, XMMatrixTranspose(XMMatrixInverse(nullptr, projection)));
}

// LightConstantData
void LightConstantData::SetPos(XMVECTOR pos)
{
    XMStoreFloat3(&this->lightPos, pos);
}

void LightConstantData::SetLightDir(XMVECTOR lightDir)
{
    lightDir = XMVector3Normalize(lightDir);
    XMStoreFloat3(&this->lightDir, lightDir);
}

void LightConstantData::SetViewProjection(XMMATRIX viewProjection, UINT idx)
{
    XMStoreFloat4x4(&this->viewProjection[idx], XMMatrixTranspose(viewProjection));
}

// MaterialConstantData
// Use linear color for gamma-correct rendering
void MaterialConstantData::SetAmbient(XMFLOAT4 ambient)
{
    XMStoreFloat3(&this->materialAmbient, XMColorSRGBToRGB(XMLoadFloat4(&ambient)));
}

void MaterialConstantData::SetSpecular(XMFLOAT4 specular)
{
    XMStoreFloat3(&this->materialSpecular, XMColorSRGBToRGB(XMLoadFloat4(&specular)));
}