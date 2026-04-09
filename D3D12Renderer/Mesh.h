#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include "D3DHelper.h"
#include "GeometryGenerator.h"
#include "TransientUploadAllocator.h"

using Microsoft::WRL::ComPtr;

class Material;

class Mesh
{
public:
    Mesh(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        TransientUploadAllocator& allocator,
        const GeometryData& geometryData)
    {
        const UINT64 vbSize = static_cast<UINT64>(geometryData.vertices.size()) * sizeof(Vertex);
        auto vbAlloc = allocator.Allocate(vbSize, sizeof(Vertex));
        D3DHelper::CreateVertexBuffer(pDevice, pCommandList, vbAlloc, m_vertexBuffer, &m_vertexBufferView, geometryData.vertices);

        m_numIndices = UINT(geometryData.indices.size());
        const UINT64 ibSize = static_cast<UINT64>(m_numIndices) * sizeof(UINT32);
        auto ibAlloc = allocator.Allocate(ibSize, sizeof(UINT32));
        D3DHelper::CreateIndexBuffer(pDevice, pCommandList, ibAlloc, m_indexBuffer, &m_indexBufferView, geometryData.indices);
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