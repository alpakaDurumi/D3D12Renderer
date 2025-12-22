#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <DirectXMath.h>

#include <vector>

#include "D3DHelper.h"
#include "ConstantData.h"

class CommandList;

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace D3DHelper;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
    XMFLOAT4 tangent;
    XMFLOAT3 normal;
};

struct InstanceData
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 inverseTranspose;
};

class Mesh
{
public:
    virtual void Render(ComPtr<ID3D12GraphicsCommandList7>& commandList) const
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(m_numIndices, 1, 0, 0, 0);
    }

    inline static Mesh MakeCube(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        UploadBuffer& uploadBuffer)
    {
        Mesh cube;

        // Create the vetex buffer
        std::vector<Vertex> cubeVertices =
        {
            // upper
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},

            // lower
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},

            // left
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

            // right
            {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

            // front
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

            // back
            {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
        };
        CreateVertexBuffer(pDevice, commandList, uploadBuffer, cube.m_vertexBuffer, &cube.m_vertexBufferView, cubeVertices);

        // Create the index buffer
        std::vector<UINT32> cubeIndices =
        {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            8, 9, 10, 8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
        };
        CreateIndexBuffer(pDevice, commandList, uploadBuffer, cube.m_indexBuffer, &cube.m_indexBufferView, cubeIndices);
        cube.m_numIndices = UINT(cubeIndices.size());

        return cube;
    }

    inline static Mesh MakeSphere(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        UploadBuffer& uploadBuffer)
    {
        Mesh sphere;

        const float radius = 1.0f;
        const UINT numStacks = 30;
        const UINT numSectors = 30;

        // Create the vetex buffer
        std::vector<Vertex> sphereVertices;
        const float dPhi = XM_PI / float(numStacks);
        const float dTheta = XM_2PI / float(numSectors);
        for (UINT i = 0; i <= numStacks; i++)
        {
            float sinPhi, cosPhi;
            XMScalarSinCos(&sinPhi, &cosPhi, i * dPhi);
            // Detect north pole and set to 0.0f for eliminating floating point error
            XMFLOAT3 stackStart(0.0f, -radius * cosPhi, i == numStacks ? 0.0f : -radius * sinPhi);
            for (UINT j = 0; j <= numSectors; j++)
            {
                float sinTheta, cosTheta;
                XMScalarSinCos(&sinTheta, &cosTheta, j * dTheta);

                Vertex v;
                v.position.x = -stackStart.z * sinTheta;
                v.position.y = stackStart.y;
                v.position.z = -stackStart.z * -cosTheta;
                v.texCoord = { float(j) / numSectors, 1.0f - float(i) / numStacks };
                if (stackStart.z == 0.0f)       // Detect pole
                {
                    v.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
                }
                else
                {
                    XMStoreFloat4(&v.tangent, XMVector3Normalize(XMVectorSet(-stackStart.z * cosTheta, 0.0f, -stackStart.z * sinTheta, 0.0f)));
                    v.tangent.w = 1.0f;
                }
                XMStoreFloat3(&v.normal, XMVector3Normalize(XMLoadFloat3(&v.position)));

                sphereVertices.push_back(v);
            }
        }
        CreateVertexBuffer(pDevice, commandList, uploadBuffer, sphere.m_vertexBuffer, &sphere.m_vertexBufferView, sphereVertices);

        // Create the index buffer
        std::vector<UINT32> sphereIndices;
        for (UINT i = 0; i < numStacks; i++)
        {
            for (UINT j = 0; j < numSectors; j++)
            {
                UINT p1 = i * (numSectors + 1) + j;
                UINT p2 = (i + 1) * (numSectors + 1) + j;
                UINT p3 = p2 + 1;
                UINT p4 = p1 + 1;
                sphereIndices.push_back(p1);
                sphereIndices.push_back(p2);
                sphereIndices.push_back(p3);
                sphereIndices.push_back(p1);
                sphereIndices.push_back(p3);
                sphereIndices.push_back(p4);
            }
        }
        CreateIndexBuffer(pDevice, commandList, uploadBuffer, sphere.m_indexBuffer, &sphere.m_indexBufferView, sphereIndices);
        sphere.m_numIndices = UINT(sphereIndices.size());

        return sphere;
    }

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    UINT m_numIndices = 0;

    MeshConstantData m_meshBufferData;
    UINT m_meshConstantBufferIndex;
    UINT m_materialConstantBufferIndex;
};

class InstancedMesh : public Mesh
{
public:
    InstancedMesh(const Mesh& mesh)
        : Mesh(mesh)
    {
    }

    void Render(ComPtr<ID3D12GraphicsCommandList7>& commandList) const override
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW pVertexBufferViews[] = { m_vertexBufferView, m_instanceBufferView };
        commandList->IASetVertexBuffers(0, 2, pVertexBufferViews);
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(m_numIndices, m_instanceCount, 0, 0, 0);
    }

    inline static InstancedMesh MakeCubeInstanced(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        UploadBuffer& uploadBuffer)
    {
        InstancedMesh cube = MakeCube(pDevice, commandList, uploadBuffer);

        std::vector<InstanceData> instances;

        for (int i = 0; i < 100; i++)
        {
            for (int j = 0; j < 100; j++)
            {
                InstanceData data;
                XMMATRIX world = XMMatrixTranslation((i - 50.0f) * 4.0f, (j - 50.0f) * 4.0f, 10.0f);
                XMStoreFloat4x4(&data.world, XMMatrixTranspose(world));
                world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
                XMStoreFloat4x4(&data.inverseTranspose, XMMatrixInverse(nullptr, world));
                instances.push_back(data);
            }
        }

        CreateVertexBuffer(pDevice, commandList, uploadBuffer, cube.m_instanceBuffer, &cube.m_instanceBufferView, instances);

        return cube;
    }

    ComPtr<ID3D12Resource> m_instanceBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView;
    UINT m_instanceCount = 10000;
};