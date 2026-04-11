#include "pch.h"
#include "Mesh.h"

#include "D3DHelper.h"

Mesh::Mesh(
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

const D3D12_VERTEX_BUFFER_VIEW& Mesh::GetVBV() const
{
    return m_vertexBufferView;
}

const D3D12_INDEX_BUFFER_VIEW& Mesh::GetIBV() const
{
    return m_indexBufferView;
}

UINT Mesh::GetNumIndices() const
{
    return m_numIndices;
}

Material* Mesh::GetMaterial() const
{
    return m_pDefaultMaterial;
}

void Mesh::SetMaterial(Material* pMat)
{
    m_pDefaultMaterial = pMat;
}