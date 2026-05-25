#pragma once

#include <string>
#include <vector>

#include <DirectXMath.h>
#include <basetsd.h>

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 texCoord;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT3 normal;
};

struct GeometryData
{
    std::string name;
    std::vector<Vertex> vertices;
    std::vector<UINT32> indices;
};
