#include "pch.h"

#include "ConstantData.h"

using namespace DirectX;

// CameraConstantData
void XM_CALLCONV CameraConstantData::SetPos(FXMVECTOR pos)
{
    XMStoreFloat3(&this->cameraPos, pos);
}

void XM_CALLCONV CameraConstantData::SetView(FXMMATRIX view)
{
    XMStoreFloat4x4(&this->view, XMMatrixTranspose(view));
    XMStoreFloat4x4(&this->invView, XMMatrixTranspose(XMMatrixInverse(nullptr, view)));
}

void XM_CALLCONV CameraConstantData::SetProjection(FXMMATRIX projection)
{
    XMStoreFloat4x4(&this->projection, XMMatrixTranspose(projection));
    XMStoreFloat4x4(&this->invProj, XMMatrixTranspose(XMMatrixInverse(nullptr, projection)));
}

// LightConstantData
void XM_CALLCONV LightConstantData::SetPos(FXMVECTOR pos)
{
    XMStoreFloat3(&this->lightPos, pos);
}

void XM_CALLCONV LightConstantData::SetLightDir(FXMVECTOR lightDir)
{
    XMStoreFloat3(&this->lightDir, XMVector3Normalize(lightDir));
}

void XM_CALLCONV LightConstantData::SetViewProjection(FXMMATRIX viewProjection, UINT idx)
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
