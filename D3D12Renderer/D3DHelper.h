#pragma once

#include <Windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6
#include <vector>

using Microsoft::WRL::ComPtr;

class ResourceLayoutTracker;

namespace D3DHelper
{
    void ThrowIfFailed(HRESULT hr);

    void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = true);

    bool CheckTearingSupport();

    void MoveCPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* handle, INT offsetInDescriptors, INT descriptorIncrementSize);
    void MoveGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE* handle, INT offsetInDescriptors, INT descriptorIncrementSize);
    void MoveCPUAndGPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle,
        INT offsetInDescriptors, INT descriptorIncrementSize);

    void DowngradeDescriptorRanges(const D3D12_DESCRIPTOR_RANGE1* src, UINT NumDescriptorRanges, std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges);
    void DowngradeRootDescriptor(const D3D12_ROOT_DESCRIPTOR1* src, D3D12_ROOT_DESCRIPTOR* dst);
    void DowngradeRootParameters(const D3D12_ROOT_PARAMETER1* src, UINT numParameters, D3D12_ROOT_PARAMETER* dst, std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges, UINT& offset);

    void CreateUploadHeap(ComPtr<ID3D12Device10>& device, UINT64 requiredSize, ComPtr<ID3D12Resource>& uploadHeap);
    void CreateDefaultHeapForBuffer(ComPtr<ID3D12Device10>& device, UINT64 size, ComPtr<ID3D12Resource>& defaultHeap);
    void CreateDefaultHeapForTexture(ComPtr<ID3D12Device10>& device, ComPtr<ID3D12Resource>& defaultHeap, UINT width, UINT height);

    void UpdateSubresources(
        ComPtr<ID3D12Device10>& device,
        ComPtr<ID3D12GraphicsCommandList7>& commandList,
        ComPtr<ID3D12Resource>& dest,
        ComPtr<ID3D12Resource>& intermediate,
        UINT64 intermediateOffset,
        UINT firstSubresource,
        UINT numSubresources,
        D3D12_SUBRESOURCE_DATA* pSrcData);

    D3D12_RESOURCE_BARRIER GetTransitionBarrier(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

    void Barrier(
        ID3D12GraphicsCommandList7* pCommandList,
        ID3D12Resource* pResource,
        D3D12_BARRIER_SYNC syncBefore,
        D3D12_BARRIER_SYNC syncAfter,
        D3D12_BARRIER_ACCESS accessBefore,
        D3D12_BARRIER_ACCESS accessAfter);

    void Barrier(
        ID3D12GraphicsCommandList7* pCommandList,
        ID3D12Resource* pResource,
        ResourceLayoutTracker& layoutTracker,
        D3D12_BARRIER_SYNC syncBefore,
        D3D12_BARRIER_SYNC syncAfter,
        D3D12_BARRIER_ACCESS accessBefore,
        D3D12_BARRIER_ACCESS accessAfter,
        D3D12_BARRIER_LAYOUT layoutAfter,
        D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange);

    D3D12_BARRIER_GROUP BufferBarrierGroup(UINT32 numBarriers, D3D12_BUFFER_BARRIER* pBarriers);
    D3D12_BARRIER_GROUP TextureBarrierGroup(UINT32 numBarriers, D3D12_TEXTURE_BARRIER* pBarriers);
    D3D12_BARRIER_GROUP GlobalBarrierGroup(UINT32 numBarriers, D3D12_GLOBAL_BARRIER* pBarriers);

    template<typename T>
    void CreateVertexBuffer(
        ComPtr<ID3D12Device10>& device,
        ComPtr<ID3D12GraphicsCommandList7>& commandList,
        ComPtr<ID3D12Resource>& vertexBuffer,
        ComPtr<ID3D12Resource>& uploadHeap,
        D3D12_VERTEX_BUFFER_VIEW* pvertexBufferView,
        std::vector<T>& vertices)
    {
        const UINT vertexBufferSize = UINT(vertices.size()) * UINT(sizeof(T));

        CreateDefaultHeapForBuffer(device, vertexBufferSize, vertexBuffer);

        CreateUploadHeap(device, vertexBufferSize, uploadHeap);

        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = vertices.data();
        vertexData.RowPitch = vertexBufferSize;
        vertexData.SlicePitch = vertexData.RowPitch;

        UpdateSubresources(device, commandList, vertexBuffer, uploadHeap, 0, 0, 1, &vertexData);

        D3D12_BUFFER_BARRIER barrier =
        {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_VERTEX_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_VERTEX_BUFFER,
            vertexBuffer.Get(),
            0,
            vertexBufferSize
        };
        D3D12_BARRIER_GROUP barrierGroups[] = { BufferBarrierGroup(1, &barrier) };
        commandList->Barrier(1, barrierGroups);

        // Initialize the vertex buffer view
        pvertexBufferView->BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        pvertexBufferView->StrideInBytes = sizeof(T);
        pvertexBufferView->SizeInBytes = vertexBufferSize;
    }

    void CreateIndexBuffer(
        ComPtr<ID3D12Device10>& device,
        ComPtr<ID3D12GraphicsCommandList7>& commandList,
        ComPtr<ID3D12Resource>& indexBuffer,
        ComPtr<ID3D12Resource>& uploadHeap,
        D3D12_INDEX_BUFFER_VIEW* pindexBufferView,
        std::vector<UINT32>& indices);

    void CreateTexture(
        ComPtr<ID3D12Device10>& device,
        ComPtr<ID3D12GraphicsCommandList7>& commandList,
        ComPtr<ID3D12Resource>& texture,
        ComPtr<ID3D12Resource>& uploadHeap,
        std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

    void CreateDepthStencilBuffer(
        ComPtr<ID3D12Device10>& device,
        UINT width,
        UINT height,
        ComPtr<ID3D12Resource>& depthStencilBuffer,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

    UINT8 GetFormatPlaneCount(ID3D12Device* pDevice, DXGI_FORMAT format);

    UINT CalcSubresourceIndex(
        UINT mipIndex,
        UINT arrayIndex,
        UINT planeIndex,
        UINT mipLevels,
        UINT arraySize);
}