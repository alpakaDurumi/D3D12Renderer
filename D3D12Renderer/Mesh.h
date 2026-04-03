#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include "D3DHelper.h"
#include "GeometryGenerator.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

class Material;

class Mesh
{
public:
    Mesh(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        UploadBuffer& uploadBuffer,
        const GeometryData& geometryData)
    {
        CreateVertexBuffer(pDevice, pCommandList, uploadBuffer, m_vertexBuffer, &m_vertexBufferView, geometryData.vertices);
        CreateIndexBuffer(pDevice, pCommandList, uploadBuffer, m_indexBuffer, &m_indexBufferView, geometryData.indices);
        m_numIndices = UINT(geometryData.indices.size());
    }

    void SetMaterial(Material* mat)
    {
        m_pDefaultMaterial = mat;
    }

    const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const
    {
        return m_vertexBufferView;
    }

    const D3D12_INDEX_BUFFER_VIEW& GetIBV() const
    {
        return m_indexBufferView;
    }

    UINT GetNumIndices() const
    {
        return m_numIndices;
    }

    Material* GetMaterial() const
    {
        return m_pDefaultMaterial;
    }

private:
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    UINT m_numIndices = 0;

    Material* m_pDefaultMaterial = nullptr;
};