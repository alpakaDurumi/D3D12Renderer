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

        m_prevS = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_currS = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_prevR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_currR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_prevT = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_currT = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    virtual ~Mesh() = default;

    void SetInitialTransform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
    {
        m_prevS = m_currS = s;

        XMVECTOR r = XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
        XMStoreFloat4(&m_prevR, r);
        m_currR = m_prevR;

        m_prevT = m_currT = t;
    }

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
        m_prevS = m_currS;
        m_prevR = m_currR;
        m_prevT = m_currT;
    }

    // Accumulate each component
    void Transform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
    {
        // S
        m_currS.x *= s.x;
        m_currS.y *= s.y;
        m_currS.z *= s.z;

        // R
        XMVECTOR currR = XMLoadFloat4(&m_currR);
        XMVECTOR deltaR = XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
        currR = XMQuaternionNormalize(XMQuaternionMultiply(currR, deltaR));
        XMStoreFloat4(&m_currR, currR);

        // T
        m_currT.x += t.x;
        m_currT.y += t.y;
        m_currT.z += t.z;
    }

    void UpdateRenderState(float alpha)
    {
        XMVECTOR prevS = XMVectorSetW(XMLoadFloat3(&m_prevS), 0.0f);
        XMVECTOR currS = XMVectorSetW(XMLoadFloat3(&m_currS), 0.0f);
        XMVECTOR prevR = XMLoadFloat4(&m_prevR);
        XMVECTOR currR = XMLoadFloat4(&m_currR);
        XMVECTOR prevT = XMVectorSetW(XMLoadFloat3(&m_prevT), 0.0f);
        XMVECTOR currT = XMVectorSetW(XMLoadFloat3(&m_currT), 0.0f);

        XMVECTOR renderS = XMVectorLerp(prevS, currS, alpha);
        XMVECTOR renderR = XMQuaternionSlerp(prevR, currR, alpha);
        XMVECTOR renderT = XMVectorLerp(prevT, currT, alpha);

        XMStoreFloat4x4(&m_renderTransform, XMMatrixAffineTransformation(renderS, XMVectorZero(), renderR, renderT));
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

    XMFLOAT3 m_prevS, m_currS;
    XMFLOAT4 m_prevR, m_currR;
    XMFLOAT3 m_prevT, m_currT;

    XMFLOAT4X4 m_renderTransform;
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