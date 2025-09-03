#pragma once

#include <Windows.h>
#include <vector>
#include <wrl/client.h>
#include "DirectXMath.h"
#include <d3d12.h>
#include "D3DHelper.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace D3DHelper;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
    XMFLOAT3 normal;
};

struct SceneConstantBuffer
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 view;
    XMFLOAT4X4 projection;
    float padding[16];
};
static_assert((sizeof(SceneConstantBuffer) % 256) == 0, "Constant Buffer size must be 256-byte aligned");

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
    static Mesh MakeCube(
        ComPtr<ID3D12Device>& device,
        ComPtr<ID3D12GraphicsCommandList>& commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE cbvSrvUavHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
        UINT cbvSrvUavDescriptorSize,
        ComPtr<ID3D12Resource>& tempUploadHeap  // default 힙으로의 복사를 위해 임시로 사용하는 upload 힙
    )
    {
        Mesh cube;
        {
            Vertex cubeVertices[] =
            {
                // upper
                {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f} },
                {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
                {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
                {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f} },

                // lower
                {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f} },
                {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f} },
                {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f} },
                {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f} },

                // left
                {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f} },
                {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
                {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f} },
                {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f} },

                // right
                {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f} },
                {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f} },
                {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
                {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f} },

                // front
                {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f} },
                {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f} },
                {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f} },
                {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f} },

                // back
                {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f} },
                {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f} },
                {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f} },
                {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f} }
            };

            //const UINT vertexBufferSize = sizeof(triangleVertices);
            const UINT vertexBufferSize = sizeof(cubeVertices);

            // Note: using upload heaps to transfer static data like vert buffers is not 
            // recommended. Every time the GPU needs it, the upload heap will be marshalled 
            // over. Please read up on Default Heap usage. An upload heap is used here for 
            // code simplicity and because there are very few verts to actually transfer.

            D3D12_HEAP_PROPERTIES heapProperties = {};
            heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
            heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProperties.CreationNodeMask = 1;
            heapProperties.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = vertexBufferSize;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc = { 1, 0 };
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            ThrowIfFailed(device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&cube.m_vertexBuffer)));

            // Copy the triangle data to the vertex buffer.
            UINT8* pVertexDataBegin;
            D3D12_RANGE readRange = { 0, 0 };       // We do not intend to read from this resource on the CPU.
            ThrowIfFailed(cube.m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
            memcpy(pVertexDataBegin, cubeVertices, sizeof(cubeVertices));
            cube.m_vertexBuffer->Unmap(0, nullptr);

            // Initialize the vertex buffer view.
            cube.m_vertexBufferView.BufferLocation = cube.m_vertexBuffer->GetGPUVirtualAddress();
            cube.m_vertexBufferView.StrideInBytes = sizeof(Vertex);
            cube.m_vertexBufferView.SizeInBytes = vertexBufferSize;
        }

        {
            // Indices
            UINT32 cubeIndices[] =
            {
                0, 1, 2, 0, 2, 3,
                4, 5, 6, 4, 6, 7,
                8, 9, 10, 8, 10, 11,
                12, 13, 14, 12, 14, 15,
                16, 17, 18, 16, 18, 19,
                20, 21, 22, 20, 22, 23
            };

            const UINT indexBufferSize = sizeof(cubeIndices);

            D3D12_HEAP_PROPERTIES heapProperties = {};
            heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
            heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProperties.CreationNodeMask = 1;
            heapProperties.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = indexBufferSize;
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc = { 1, 0 };
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            ThrowIfFailed(device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&cube.m_indexBuffer)));

            UINT8* pIndexDataBegin;
            D3D12_RANGE readRange = { 0, 0 };
            ThrowIfFailed(cube.m_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
            memcpy(pIndexDataBegin, cubeIndices, sizeof(cubeIndices));
            cube.m_indexBuffer->Unmap(0, nullptr);

            cube.m_indexBufferView.BufferLocation = cube.m_indexBuffer->GetGPUVirtualAddress();
            cube.m_indexBufferView.SizeInBytes = indexBufferSize;
            cube.m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
        }

        // Create the constant buffer
        {
            D3D12_HEAP_PROPERTIES heapProperties = {};
            heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
            heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProperties.CreationNodeMask = 1;
            heapProperties.VisibleNodeMask = 1;

            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = sizeof(SceneConstantBuffer);
            resourceDesc.Height = 1;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            resourceDesc.SampleDesc = { 1, 0 };
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            ThrowIfFailed(device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &resourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&cube.m_constantBuffer)));

            // Create constant buffer view
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = cube.m_constantBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = sizeof(SceneConstantBuffer);

            device->CreateConstantBufferView(&cbvDesc, cbvSrvUavHandle);
            cube.m_gpuHandles.push_back(gpuHandle);
            MoveCPUAndGPUDescriptorHandle(&cbvSrvUavHandle, &gpuHandle, 1, cbvSrvUavDescriptorSize);

            // Do not unmap this until app close
            D3D12_RANGE readRange = { 0, 0 };
            ThrowIfFailed(cube.m_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&cube.m_pCbvDataBegin)));
            memcpy(cube.m_pCbvDataBegin, &cube.m_constantBufferData, sizeof(cube.m_constantBufferData));
        }

        // Create the texture
        {
            D3D12_HEAP_PROPERTIES heapProperties = {};
            heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            heapProperties.CreationNodeMask = 1;
            heapProperties.VisibleNodeMask = 1;

            // Describe and create a Texture2D.
            D3D12_RESOURCE_DESC textureDesc = {};
            textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            textureDesc.Alignment = 0;
            textureDesc.Width = TextureWidth;
            textureDesc.Height = TextureHeight;
            textureDesc.DepthOrArraySize = 1;
            textureDesc.MipLevels = 1;
            textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDesc.SampleDesc = { 1, 0 };
            textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            ThrowIfFailed(device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &textureDesc,
                D3D12_RESOURCE_STATE_COPY_DEST,
                nullptr,
                IID_PPV_ARGS(&cube.m_texture)));

            // Calculate required size for data upload
            //const auto desc = m_texture->GetDesc();
            // 나중에 Texture2D 클래스를 만들어서 이 정보들을 멤버로 유지하도록 하자
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts = {};
            UINT numRows = 0;
            UINT64 rowSizeInBytes = 0;
            UINT64 requiredSize = 0;
            //ID3D12Device* pDevice = nullptr;
            //m_texture->GetDevice(IID_ID3D12Device, reinterpret_cast<void**>(&pDevice));
            device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layouts, &numRows, &rowSizeInBytes, &requiredSize);
            //pDevice->Release();

            heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC uploadDesc = {};
            uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            uploadDesc.Alignment = 0;
            uploadDesc.Width = requiredSize;
            uploadDesc.Height = 1;
            uploadDesc.DepthOrArraySize = 1;
            uploadDesc.MipLevels = 1;
            uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
            uploadDesc.SampleDesc = { 1, 0 };
            uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            // Create the GPU upload buffer.
            ThrowIfFailed(device->CreateCommittedResource(
                &heapProperties,
                D3D12_HEAP_FLAG_NONE,
                &uploadDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&tempUploadHeap)));

            // Copy data to the intermediate upload heap and then schedule a copy 
            // from the upload heap to the Texture2D.
            std::vector<UINT8> texture = GenerateTextureData(TextureWidth, TextureHeight, TexturePixelSize);

            D3D12_SUBRESOURCE_DATA textureData = {};
            textureData.pData = &texture[0];
            textureData.RowPitch = TextureWidth * TexturePixelSize;
            textureData.SlicePitch = textureData.RowPitch * TextureHeight;

            //UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
            D3D12_RANGE readRange = { 0, 0 };   // do not read from CPU. only write
            UINT8* pData;
            ThrowIfFailed(tempUploadHeap->Map(0, &readRange, reinterpret_cast<void**>(&pData)));

            // Assume that NumSubResources is 1
            // Data need for memcpy
            //D3D12_MEMCPY_DEST destData = {
            //    pData + layouts.Offset,
            //    layouts.Footprint.RowPitch,
            //    SIZE_T(layouts.Footprint.RowPitch) * SIZE_T(numRows)
            //};

            for (UINT z = 0; z < layouts.Footprint.Depth; ++z)
            {
                auto pDestSlice = static_cast<BYTE*>(pData + layouts.Offset) + SIZE_T(layouts.Footprint.RowPitch) * SIZE_T(numRows) * z;
                auto pSrcSlice = static_cast<const BYTE*>(textureData.pData) + textureData.SlicePitch * LONG_PTR(z);
                for (UINT y = 0; y < numRows; ++y)
                {
                    memcpy(pDestSlice + layouts.Footprint.RowPitch * y,
                        pSrcSlice + textureData.RowPitch * LONG_PTR(y),
                        rowSizeInBytes);
                }
            }

            tempUploadHeap->Unmap(0, nullptr);

            // Copy from upload heap to default heap
            D3D12_TEXTURE_COPY_LOCATION dst = {};
            dst.pResource = cube.m_texture.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION src = {};
            src.pResource = tempUploadHeap.Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = layouts;

            commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

            // Change resource state
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = cube.m_texture.Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
            commandList->ResourceBarrier(1, &barrier);

            // Describe and create a SRV for the texture.
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = textureDesc.Format;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Texture2D.MipLevels = 1;

            device->CreateShaderResourceView(cube.m_texture.Get(), &srvDesc, cbvSrvUavHandle);
            cube.m_gpuHandles.push_back(gpuHandle);
            MoveCPUAndGPUDescriptorHandle(&cbvSrvUavHandle, &gpuHandle, 1, cbvSrvUavDescriptorSize);
        }

        return cube;
    }

    ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    ComPtr<ID3D12Resource> m_constantBuffer;
    SceneConstantBuffer m_constantBufferData;
    UINT8* m_pCbvDataBegin;

    ComPtr<ID3D12Resource> m_texture;

    std::vector< D3D12_GPU_DESCRIPTOR_HANDLE> m_gpuHandles;

    static const UINT TextureWidth = 256;
    static const UINT TextureHeight = 256;
    static const UINT TexturePixelSize = 4;    // The number of bytes used to represent a pixel in the texture.
};