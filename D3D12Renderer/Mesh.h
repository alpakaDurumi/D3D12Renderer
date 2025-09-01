#pragma once

#include <Windows.h>
#include <vector>
#include <wrl/client.h>
#include "DirectXMath.h"
#include <d3d12.h>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
    XMFLOAT3 normal;
};

struct SceneConstantBuffer
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 view;
    XMFLOAT4X4 projection;
    float padding[16];
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

class Mesh {
public:
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    ComPtr<ID3D12Resource> m_constantBuffer;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;
    ComPtr<ID3D12Resource> m_texture;
};