#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <DirectXMath.h>

#include <vector>
#include <cmath>

#include "D3DHelper.h"
#include "ConstantData.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace D3DHelper;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
    XMFLOAT3 normal;
};

struct InstanceData
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 inverseTranspose;
};

// Generate a simple black and white checkerboard texture.
inline std::vector<UINT8> GenerateTextureData(UINT textureWidth, UINT textureHeight, UINT texturePixelSize)
{
    const UINT rowPitch = textureWidth * texturePixelSize;
    const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
    const UINT cellHeight = textureWidth >> 3;    // The height of a cell in the checkerboard texture.
    const UINT textureSize = rowPitch * textureHeight;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += texturePixelSize)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 0x00;        // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0x00;    // B
            pData[n + 3] = 0xff;    // A
        }
        else
        {
            pData[n] = 0xff;        // R
            pData[n + 1] = 0xff;    // G
            pData[n + 2] = 0xff;    // B
            pData[n + 3] = 0xff;    // A
        }
    }

    return data;
}

class Mesh
{
public:
    virtual void Render(ComPtr<ID3D12GraphicsCommandList>& commandList) const
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(m_numIndices, 1, 0, 0, 0);
    }

    inline static Mesh MakeCube(
        ComPtr<ID3D12Device>& device,
        ComPtr<ID3D12GraphicsCommandList>& commandList,
        ComPtr<ID3D12Resource>& vertexBufferUploadHeap,
        ComPtr<ID3D12Resource>& indexBufferUploadHeap,
        ComPtr<ID3D12Resource>& textureUploadHeap,
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle)
    {
        Mesh cube;

        // Create the vetex buffer
        {
            std::vector<Vertex> cubeVertices =
            {
                // upper
                {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 1.0f, 0.0f} },
                {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
                {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 1.0f, 0.0f} },
                {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 1.0f, 0.0f} },

                // lower
                {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, -1.0f, 0.0f} },
                {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, -1.0f, 0.0f} },
                {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, -1.0f, 0.0f} },
                {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, -1.0f, 0.0f} },

                // left
                {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f} },
                {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f} },
                {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f} },
                {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f} },

                // right
                {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },
                {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
                {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f} },
                {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f} },

                // front
                {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },
                {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
                {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f} },
                {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f} },

                // back
                {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f} },
                {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
                {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f} },
                {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f} }
            };
            CreateVertexBuffer(device, commandList, cube.m_vertexBuffer, vertexBufferUploadHeap, &cube.m_vertexBufferView, cubeVertices);
        }

        // Create the index buffer
        {
            std::vector<UINT32> cubeIndices =
            {
                0, 1, 2, 0, 2, 3,
                4, 5, 6, 4, 6, 7,
                8, 9, 10, 8, 10, 11,
                12, 13, 14, 12, 14, 15,
                16, 17, 18, 16, 18, 19,
                20, 21, 22, 20, 22, 23
            };
            CreateIndexBuffer(device, commandList, cube.m_indexBuffer, indexBufferUploadHeap, &cube.m_indexBufferView, cubeIndices);
            cube.m_numIndices = UINT(cubeIndices.size());
        }

        // Create the texture
        {
            CreateDefaultHeapForTexture(device, cube.m_texture, TextureWidth, TextureHeight);

            // Calculate required size for data upload
            D3D12_RESOURCE_DESC desc = cube.m_texture->GetDesc();
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 requiredSize = 0;
            device->GetCopyableFootprints(&desc, 0, 1, 0, &layouts, &numRows, &rowSizeInBytes, &requiredSize);

            CreateUploadHeap(device, requiredSize, textureUploadHeap);

            // 텍스처 데이터는 인자로 받도록 수정하기
            std::vector<UINT8> texture = GenerateTextureData(TextureWidth, TextureHeight, TexturePixelSize);
            D3D12_SUBRESOURCE_DATA textureData = {};
            textureData.pData = &texture[0];
            textureData.RowPitch = TextureWidth * TexturePixelSize;
            textureData.SlicePitch = textureData.RowPitch * TextureHeight;

            UpdateSubResources(device, commandList, cube.m_texture, textureUploadHeap, &textureData);

            // Change resource state
            D3D12_RESOURCE_BARRIER barrier = GetTransitionBarrier(cube.m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            commandList->ResourceBarrier(1, &barrier);

            // Describe and create a SRV for the texture.
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;

            device->CreateShaderResourceView(cube.m_texture.Get(), &srvDesc, srvCpuHandle);
            //MoveCPUDescriptorHandle(&cpuHandle, 1, m_cbvSrvUavDescriptorSize);
        }

        return cube;
    }

    inline static Mesh MakeSphere(
        ComPtr<ID3D12Device>& device,
        ComPtr<ID3D12GraphicsCommandList>& commandList,
        ComPtr<ID3D12Resource>& vertexBufferUploadHeap,
        ComPtr<ID3D12Resource>& indexBufferUploadHeap,
        ComPtr<ID3D12Resource>& textureUploadHeap,
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle)
    {
        Mesh sphere;

        const float radius = 1.0f;
        const UINT numStacks = 30;
        const UINT numSectors = 30;

        // Create the vetex buffer
        {
            std::vector<Vertex> sphereVertices;
            const float dPhi = XM_PI / float(numStacks);
            const float dTheta = XM_2PI / float(numSectors);
            for (UINT i = 0; i <= numStacks; i++)
            {
                float sinPhi, cosPhi;
                XMScalarSinCos(&sinPhi, &cosPhi, i * dPhi);
                XMFLOAT3 stackStart(0.0f, 0.0f - radius * cosPhi, -radius * sinPhi);
                for (UINT j = 0; j <= numSectors; j++)
                {
                    float sinTheta, cosTheta;
                    XMScalarSinCos(&sinTheta, &cosTheta, j * dTheta);

                    Vertex v;
                    v.position.x = -stackStart.z * sinTheta;
                    v.position.y = stackStart.y;
                    v.position.z = -stackStart.z * -cosTheta;
                    v.texCoord = { float(j) / numSectors, 1.0f - float(i) / numStacks };
                    XMStoreFloat3(&v.normal, XMVector3Normalize(XMLoadFloat3(&v.position)));

                    sphereVertices.push_back(v);
                }
            }
            CreateVertexBuffer(device, commandList, sphere.m_vertexBuffer, vertexBufferUploadHeap, &sphere.m_vertexBufferView, sphereVertices);
        }

        // Create the index buffer
        {
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
            CreateIndexBuffer(device, commandList, sphere.m_indexBuffer, indexBufferUploadHeap, &sphere.m_indexBufferView, sphereIndices);
            sphere.m_numIndices = UINT(sphereIndices.size());
        }

        // Create the texture
        {
            CreateDefaultHeapForTexture(device, sphere.m_texture, TextureWidth, TextureHeight);

            // Calculate required size for data upload
            D3D12_RESOURCE_DESC desc = sphere.m_texture->GetDesc();
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 requiredSize = 0;
            device->GetCopyableFootprints(&desc, 0, 1, 0, &layouts, &numRows, &rowSizeInBytes, &requiredSize);

            CreateUploadHeap(device, requiredSize, textureUploadHeap);

            // 텍스처 데이터는 인자로 받도록 수정하기
            std::vector<UINT8> texture = GenerateTextureData(TextureWidth, TextureHeight, TexturePixelSize);
            D3D12_SUBRESOURCE_DATA textureData = {};
            textureData.pData = &texture[0];
            textureData.RowPitch = TextureWidth * TexturePixelSize;
            textureData.SlicePitch = textureData.RowPitch * TextureHeight;

            UpdateSubResources(device, commandList, sphere.m_texture, textureUploadHeap, &textureData);

            // Change resource state
            D3D12_RESOURCE_BARRIER barrier = GetTransitionBarrier(sphere.m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            commandList->ResourceBarrier(1, &barrier);

            // Describe and create a SRV for the texture.
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = desc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;

            device->CreateShaderResourceView(sphere.m_texture.Get(), &srvDesc, srvCpuHandle);
            //MoveCPUDescriptorHandle(&cpuHandle, 1, m_cbvSrvUavDescriptorSize);
        }

        return sphere;
    }

    static const UINT TextureWidth = 256;
    static const UINT TextureHeight = 256;
    static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
    UINT m_numIndices = 0;

    SceneConstantData m_constantBufferData;
    MaterialConstantData m_materialConstantBufferData;
    UINT m_sceneConstantBufferIndex;
    UINT m_materialConstantBufferIndex;

    ComPtr<ID3D12Resource> m_texture;
};

class InstancedMesh : public Mesh
{
public:
    InstancedMesh(const Mesh& mesh)
        : Mesh(mesh)
    {
    }

    void Render(ComPtr<ID3D12GraphicsCommandList>& commandList) const override
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D12_VERTEX_BUFFER_VIEW pVertexBufferViews[] = { m_vertexBufferView, m_instanceBufferView };
        commandList->IASetVertexBuffers(0, 2, pVertexBufferViews);
        commandList->IASetIndexBuffer(&m_indexBufferView);
        commandList->DrawIndexedInstanced(m_numIndices, m_instanceCount, 0, 0, 0);
    }

    inline static InstancedMesh MakeCubeInstanced(
        ComPtr<ID3D12Device>& device,
        ComPtr<ID3D12GraphicsCommandList>& commandList,
        ComPtr<ID3D12Resource>& vertexBufferUploadHeap,
        ComPtr<ID3D12Resource>& indexBufferUploadHeap,
        ComPtr<ID3D12Resource>& textureUploadHeap,
        ComPtr<ID3D12Resource>& instanceUploadHeap,
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle
    )
    {
        InstancedMesh cube = MakeCube(device, commandList, vertexBufferUploadHeap, indexBufferUploadHeap, textureUploadHeap, srvCpuHandle);

        std::vector<InstanceData> instances;

        for (int i = 0; i < 100; i++)
        {
            for (int j = 0; j < 100; j++)
            {
                for (int k = 0; k < 100; k++)
                {
                    InstanceData data;
                    XMMATRIX world = XMMatrixTranslation((i - 50.0f) * 4.0f, (j - 50.0f) * 4.0f, (k - 50.0f) * 4.0f);
                    XMStoreFloat4x4(&data.world, XMMatrixTranspose(world));
                    world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
                    XMStoreFloat4x4(&data.inverseTranspose, XMMatrixInverse(nullptr, world));
                    instances.push_back(data);
                }
            }
        }

        CreateVertexBuffer(device, commandList, cube.m_instanceBuffer, instanceUploadHeap, &cube.m_instanceBufferView, instances);

        return cube;
    }

    ComPtr<ID3D12Resource> m_instanceBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_instanceBufferView;
    UINT m_instanceCount = 1'000'000;
};