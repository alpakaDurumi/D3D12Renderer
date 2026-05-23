#include "pch.h"
#include "Mesh.h"

#include "D3DHelper.h"
#include "GeometryData.h"
#include "TransientUploadAllocator.h"
#include "UploadAllocation.h"

using namespace D3DHelper;

Mesh::Mesh(
    ID3D12Device10* pDevice,
    ID3D12GraphicsCommandList7* pCommandList,
    TransientUploadAllocator& allocator,
    const GeometryData& geometryData)
{
    // Vertex Buffer
    const UINT64 vertexBufferSize = static_cast<UINT64>(geometryData.vertices.size()) * sizeof(Vertex);
    auto vbAlloc = allocator.Allocate(vertexBufferSize, sizeof(Vertex));

    m_vertexBuffer = Buffer(pDevice, vertexBufferSize);

    D3D12_SUBRESOURCE_DATA vertexData = {};
    vertexData.pData = geometryData.vertices.data();
    vertexData.RowPitch = vertexBufferSize;
    vertexData.SlicePitch = vertexData.RowPitch;

    UpdateSubresources(pDevice, pCommandList, m_vertexBuffer.Get(), vbAlloc.pResource, vbAlloc.offset, vbAlloc.cpuPtr, 0, 1, &vertexData);

    // Index Buffer
    m_numIndices = UINT(geometryData.indices.size());
    const UINT64 indexBufferSize = static_cast<UINT64>(m_numIndices) * sizeof(UINT32);
    auto ibAlloc = allocator.Allocate(indexBufferSize, sizeof(UINT32));

    m_indexBuffer = Buffer(pDevice, indexBufferSize);

    D3D12_SUBRESOURCE_DATA indexData = {};
    indexData.pData = geometryData.indices.data();
    indexData.RowPitch = indexBufferSize;
    indexData.SlicePitch = indexData.RowPitch;

    UpdateSubresources(pDevice, pCommandList, m_indexBuffer.Get(), ibAlloc.pResource, ibAlloc.offset, ibAlloc.cpuPtr, 0, 1, &indexData);

    // Barrier
    D3D12_BUFFER_BARRIER barriers[2] = {
        {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_VERTEX_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_VERTEX_BUFFER,
            m_vertexBuffer.Get(),
            0,
            UINT64_MAX
        },
        {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_INDEX_INPUT,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_INDEX_BUFFER,
            m_indexBuffer.Get(),
            0,
            UINT64_MAX
        }
    };
    D3D12_BARRIER_GROUP barrierGroups[] = { BufferBarrierGroup(2, barriers) };
    pCommandList->Barrier(1, barrierGroups);

    // Initialize VBV, IBV
    m_vbv.BufferLocation = m_vertexBuffer.Get()->GetGPUVirtualAddress();
    m_vbv.SizeInBytes = static_cast<UINT>(vertexBufferSize);
    m_vbv.StrideInBytes = static_cast<UINT>(sizeof(Vertex));

    m_ibv.BufferLocation = m_indexBuffer.Get()->GetGPUVirtualAddress();
    m_ibv.SizeInBytes = static_cast<UINT>(indexBufferSize);
    m_ibv.Format = DXGI_FORMAT_R32_UINT;
}

const D3D12_VERTEX_BUFFER_VIEW& Mesh::GetVbv() const
{
    return m_vbv;
}

const D3D12_INDEX_BUFFER_VIEW& Mesh::GetIbv() const
{
    return m_ibv;
}

UINT Mesh::GetNumIndices() const
{
    return m_numIndices;
}

MaterialHandle Mesh::GetMaterial() const
{
    return m_material;
}

void Mesh::SetMaterial(MaterialHandle handle)
{
    m_material = handle;
}