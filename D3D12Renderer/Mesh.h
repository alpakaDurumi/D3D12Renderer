#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include "GeometryData.h"
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
        const GeometryData& geometryData);

    const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const;
    const D3D12_INDEX_BUFFER_VIEW& GetIBV() const;
    UINT GetNumIndices() const;
    Material* GetMaterial() const;

    void SetMaterial(Material* mat);

private:
    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    UINT m_numIndices = 0;

    Material* m_pDefaultMaterial = nullptr;
};