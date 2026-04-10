#pragma once

#include <Windows.h>

#include <DirectXMath.h>

#include <vector>
#include <string>

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT2 texCoord;
    DirectX::XMFLOAT4 tangent;
    DirectX::XMFLOAT3 normal;
};

struct GeometryData
{
    std::wstring name;
    std::vector<Vertex> vertices;
    std::vector<UINT32> indices;
};