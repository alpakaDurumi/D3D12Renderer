#pragma once

#include <Windows.h>

#include <DirectXMath.h>

struct InstanceData
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 inverseTranspose;
    UINT materialIndex;
};