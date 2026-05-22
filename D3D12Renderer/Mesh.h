#pragma once

#include <minwindef.h>

#include <d3d12.h>

#include "GeometryData.h"
#include "TransientUploadAllocator.h"
#include "SceneHandles.h"
#include "Buffer.h"

class Mesh
{
public:
    Mesh(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        TransientUploadAllocator& allocator,
        const GeometryData& geometryData);

    const D3D12_VERTEX_BUFFER_VIEW& GetVbv() const;
    const D3D12_INDEX_BUFFER_VIEW& GetIbv() const;
    UINT GetNumIndices() const;

    MaterialHandle GetMaterial() const;
    void SetMaterial(MaterialHandle handle);

private:
    Buffer m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbv;

    Buffer m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_ibv;
    UINT m_numIndices = 0;

    MaterialHandle m_material;
};