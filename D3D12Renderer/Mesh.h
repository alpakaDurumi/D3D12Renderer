#pragma once

#include <vector>

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <d3d12.h>
#include <minwindef.h>

#include "Buffer.h"
#include "SceneHandles.h"

struct GeometryData;
class TransientUploadAllocator;

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

    const DirectX::BoundingSphere& GetBoundingSphere() const;

    const std::vector<DirectX::XMFLOAT3>& GetPositions() const;
    const std::vector<UINT32>& GetIndices() const;

private:
    Buffer m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vbv;

    Buffer m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_ibv;
    UINT m_numIndices = 0;

    MaterialHandle m_material;

    DirectX::BoundingSphere m_boundingSphere;

    std::vector<DirectX::XMFLOAT3> m_positions;
    std::vector<UINT32> m_indices;
};
