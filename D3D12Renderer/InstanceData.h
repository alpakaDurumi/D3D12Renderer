#pragma once

#include <DirectXMath.h>
#include <minwindef.h>

struct InstanceData
{
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 inverseTranspose;
    UINT materialIndex;
};
