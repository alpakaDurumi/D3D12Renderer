#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include <vector>
#include <memory>

#include "D3DHelper.h"
#include "CommandList.h"
#include "ConstantData.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"
#include "SharedConfig.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

class Mesh
{
public:
    Mesh(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        UploadBuffer& uploadBuffer,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources,
        const GeometryData& geometryData,
        TextureAddressingMode textureAddressingMode = TextureAddressingMode::WRAP)
        : m_textureAddressingMode(textureAddressingMode)
    {
        CreateVertexBuffer(pDevice, commandList, uploadBuffer, m_vertexBuffer, &m_vertexBufferView, geometryData.vertices);
        CreateIndexBuffer(pDevice, commandList, uploadBuffer, m_indexBuffer, &m_indexBufferView, geometryData.indices);
        m_numIndices = UINT(geometryData.indices.size());

        // Create constant buffers
        for (UINT i = 0; i < frameResources.size(); ++i)
        {
            FrameResource& frameResource = *frameResources[i];

            // Set index only at first iteration because indices are same in each FrameResource
            if (i == 0) m_meshConstantBufferIndex = UINT(frameResource.m_meshConstantBuffers.size());
            frameResource.m_meshConstantBuffers.push_back(std::make_unique<MeshCB>(pDevice));

            if (i == 0) m_materialConstantBufferIndex = 0;
        }

        m_prevTransform = XMMatrixIdentity();
        m_currTransform = XMMatrixIdentity();
    }

    virtual ~Mesh() = default;

    virtual void Render(ComPtr<ID3D12GraphicsCommandList7>& commandList) const
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(m_numIndices, 1, 0, 0, 0);
    }

    void UpdateMeshConstantBuffer(FrameResource& frameResource) const
    {
        frameResource.m_meshConstantBuffers[m_meshConstantBufferIndex]->Update(&m_meshConstantData);
    }

    void SnapshotState()
    {
        m_prevTransform = m_currTransform;
    }

    void Transform(XMMATRIX transform)
    {
        m_currTransform *= transform;
    }

    void UpdateRenderState(float alpha)
    {
        XMVECTOR prevS, prevR, prevT;
        XMMatrixDecompose(&prevS, &prevR, &prevT, m_prevTransform);

        XMVECTOR currS, currR, currT;
        XMMatrixDecompose(&currS, &currR, &currT, m_currTransform);

        XMVECTOR s = XMVectorLerp(prevS, currS, alpha);
        XMVECTOR r = XMQuaternionSlerp(prevR, currR, alpha);
        XMVECTOR t = XMVectorLerp(prevT, currT, alpha);
        
        m_renderTransform = XMMatrixAffineTransformation(s, XMVectorZero(), r, t);
    }

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    UINT m_numIndices = 0;

    MeshConstantData m_meshConstantData;
    UINT m_meshConstantBufferIndex;

    UINT m_materialConstantBufferIndex;

    TextureAddressingMode m_textureAddressingMode;

    XMMATRIX m_prevTransform;
    XMMATRIX m_currTransform;
    XMMATRIX m_renderTransform;
};

class InstancedMesh : public Mesh
{
public:
    InstancedMesh(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        UploadBuffer& uploadBuffer,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources,
        const GeometryData& geometryData,
        const std::vector<InstanceData>& instanceData,
        TextureAddressingMode textureAddressingMode = TextureAddressingMode::WRAP)
        : Mesh(pDevice, commandList, uploadBuffer, frameResources, geometryData, textureAddressingMode)
    {
        CreateVertexBuffer(pDevice, commandList, uploadBuffer, m_instanceBuffer, &m_instanceBufferView, instanceData);
        m_instanceCount = UINT(instanceData.size());
    }

    void Render(ComPtr<ID3D12GraphicsCommandList7>& commandList) const override
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW pVertexBufferViews[] = { m_vertexBufferView, m_instanceBufferView };
        commandList->IASetVertexBuffers(0, 2, pVertexBufferViews);
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(m_numIndices, m_instanceCount, 0, 0, 0);
    }

    ComPtr<ID3D12Resource> m_instanceBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView;
    UINT m_instanceCount;
};