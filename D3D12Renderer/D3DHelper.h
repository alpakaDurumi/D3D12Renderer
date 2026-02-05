#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6

#include <vector>

#include "CommandList.h"
#include "UploadBuffer.h"
#include "DescriptorAllocation.h"
#include "SharedConfig.h"

using Microsoft::WRL::ComPtr;

class ResourceLayoutTracker;

namespace D3DHelper
{
    void ThrowIfFailed(HRESULT hr);

    void GetHardwareAdapter(_In_ IDXGIFactory1* pFactory, _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter, bool requestHighPerformanceAdapter = true);

    bool CheckTearingSupport();

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const D3D12_CPU_DESCRIPTOR_HANDLE& handle, INT offsetInDescriptors, INT descriptorIncrementSize);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const D3D12_GPU_DESCRIPTOR_HANDLE& handle, INT offsetInDescriptors, INT descriptorIncrementSize);

    void DowngradeDescriptorRanges(const D3D12_DESCRIPTOR_RANGE1* src, UINT NumDescriptorRanges, std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges);
    void DowngradeRootDescriptor(const D3D12_ROOT_DESCRIPTOR1* src, D3D12_ROOT_DESCRIPTOR* dst);
    void DowngradeRootParameters(const D3D12_ROOT_PARAMETER1* src, UINT numParameters, D3D12_ROOT_PARAMETER* dst, std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges, UINT& offset);

    void CreateUploadHeap(ID3D12Device10* pDevice, UINT64 requiredSize, ComPtr<ID3D12Resource>& uploadHeap);
    void CreateDefaultHeapForBuffer(ID3D12Device10* pDevice, UINT64 size, ComPtr<ID3D12Resource>& defaultHeap);
    void CreateDefaultHeapForTexture(ID3D12Device10* pDevice, UINT width, UINT height, ComPtr<ID3D12Resource>& defaultHeap);

    void UpdateSubresources(
        ID3D12Device* pDevice,
        CommandList& commandList,
        ID3D12Resource* pDest,
        ID3D12Resource* pIntermediate,
        UINT64 intermediateOffset,
        UINT firstSubresource,
        UINT numSubresources,
        D3D12_SUBRESOURCE_DATA* pSrcData);

    void UpdateSubresources(
        ID3D12Device* pDevice,
        CommandList& commandList,
        ID3D12Resource* pDest,
        UploadBuffer::Allocation& uploadAllocation,
        UINT firstSubresource,
        UINT numSubresources,
        D3D12_SUBRESOURCE_DATA* pSrcData);

    D3D12_RESOURCE_BARRIER GetTransitionBarrier(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

    D3D12_BARRIER_GROUP BufferBarrierGroup(UINT32 numBarriers, D3D12_BUFFER_BARRIER* pBarriers);
    D3D12_BARRIER_GROUP TextureBarrierGroup(UINT32 numBarriers, D3D12_TEXTURE_BARRIER* pBarriers);
    D3D12_BARRIER_GROUP GlobalBarrierGroup(UINT32 numBarriers, D3D12_GLOBAL_BARRIER* pBarriers);

    template<typename T>
    void CreateVertexBuffer(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        UploadBuffer& uploadBuffer,
        ComPtr<ID3D12Resource>& vertexBuffer,
        D3D12_VERTEX_BUFFER_VIEW* pVertexBufferView,
        const std::vector<T>& vertices)
    {
        const UINT vertexBufferSize = UINT(vertices.size()) * UINT(sizeof(T));

        CreateDefaultHeapForBuffer(pDevice, vertexBufferSize, vertexBuffer);

        auto uploadAllocation = uploadBuffer.Allocate(vertexBufferSize, sizeof(T));

        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = vertices.data();
        vertexData.RowPitch = vertexBufferSize;
        vertexData.SlicePitch = vertexData.RowPitch;

        UpdateSubresources(pDevice, commandList, vertexBuffer.Get(), uploadAllocation, 0, 1, &vertexData);

        commandList.Barrier(
            vertexBuffer.Get(),
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_VERTEX_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_VERTEX_BUFFER);

        // Initialize the vertex buffer view
        pVertexBufferView->BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        pVertexBufferView->StrideInBytes = sizeof(T);
        pVertexBufferView->SizeInBytes = vertexBufferSize;
    }

    void CreateIndexBuffer(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        UploadBuffer& uploadBuffer,
        ComPtr<ID3D12Resource>& indexBuffer,
        D3D12_INDEX_BUFFER_VIEW* pindexBufferView,
        const std::vector<UINT32>& indices);

    void CreateDepthStencilBuffer(
        ID3D12Device10* pDevice,
        UINT width,
        UINT height,
        ComPtr<ID3D12Resource>& depthStencilBuffer,
        DescriptorAllocation& dsvAllocation,
        UINT16 arraySize = 1);

    void CreateSRVForShadow(
        ID3D12Device10* pDevice,
        ID3D12Resource* pResource,
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle,
        LightType type);

    void CreateSampler(
        ID3D12Device* pDevice,
        TextureFiltering filtering,
        TextureAddressingMode addressingMode,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

    UINT16 GetRequiredArraySize(LightType type);

    UINT8 GetFormatPlaneCount(ID3D12Device* pDevice, DXGI_FORMAT format);

    UINT CalcSubresourceIndex(
        UINT mipIndex,
        UINT arrayIndex,
        UINT planeIndex,
        UINT mipLevels,
        UINT arraySize);

    void ConvertToDDS(
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage);
}