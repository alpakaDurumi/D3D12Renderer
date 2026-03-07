#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include "D3DHelper.h"
#include "CommandList.h"
#include "GeometryGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

class Material;

class Mesh
{
public:
    Mesh(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        UploadBuffer& uploadBuffer,
        const GeometryData& geometryData)
    {
        CreateVertexBuffer(pDevice, commandList, uploadBuffer, m_vertexBuffer, &m_vertexBufferView, geometryData.vertices);
        CreateIndexBuffer(pDevice, commandList, uploadBuffer, m_indexBuffer, &m_indexBufferView, geometryData.indices);
        m_numIndices = UINT(geometryData.indices.size());
    }

    void SetMaterial(Material* mat)
    {
        m_pDefaultMaterial = mat;
    }

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    UINT m_numIndices = 0;

    Material* m_pDefaultMaterial = nullptr;
};