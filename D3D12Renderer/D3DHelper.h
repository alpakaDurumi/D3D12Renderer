#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6

#include <vector>
#include <string>

#include "SharedConfig.h"
#include "UploadAllocation.h"
#include "GeometryData.h"

using Microsoft::WRL::ComPtr;

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

    D3D12_RESOURCE_DESC1 GetBufferDesc(UINT64 width, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    D3D12_RESOURCE_DESC1 GetTexture2DDesc(UINT64 width, UINT height, UINT16 arraySize, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);
    D3D12_RESOURCE_DESC1 GetTexture3DDesc(UINT64 width, UINT height, UINT16 arraySize, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

    D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc(DXGI_FORMAT format, UINT mipSlice, UINT planeSlice = 0);
    D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc2DArray(DXGI_FORMAT format, UINT mipSlice, UINT firstArraySlice, UINT arraySize, UINT planeSlice = 0);

    D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc(DXGI_FORMAT format, UINT mipLevels, UINT planeSlice = 0);
    D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc2DArray(DXGI_FORMAT format, UINT mipLevels, UINT arraySize, UINT planeSlice = 0);
    D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc3D(DXGI_FORMAT format, UINT mipLevels);
    D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDescCube(DXGI_FORMAT format, UINT mipLevels);

    D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc(DXGI_FORMAT format, D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE);
    D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc2DArray(DXGI_FORMAT format, UINT arraySlice, D3D12_DSV_FLAGS flags = D3D12_DSV_FLAG_NONE);

    void CreateUploadBuffer(ID3D12Device10* pDevice, UINT64 requiredSize, ComPtr<ID3D12Resource>& uploadBuffer);
    void CreateDefaultBuffer(ID3D12Device10* pDevice, UINT64 size, ComPtr<ID3D12Resource>& defaultBuffer);

    void CreateCBV(ID3D12Device10* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr, UINT size, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

    void UpdateSubresources(
        ID3D12Device* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        ID3D12Resource* pDest,
        UploadAllocation intermediate,
        UINT firstSubresource,
        UINT numSubresources,
        D3D12_SUBRESOURCE_DATA* pSrcData);

    D3D12_RESOURCE_BARRIER GetTransitionBarrier(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

    D3D12_BARRIER_GROUP BufferBarrierGroup(UINT32 numBarriers, D3D12_BUFFER_BARRIER* pBarriers);
    D3D12_BARRIER_GROUP TextureBarrierGroup(UINT32 numBarriers, D3D12_TEXTURE_BARRIER* pBarriers);
    D3D12_BARRIER_GROUP GlobalBarrierGroup(UINT32 numBarriers, D3D12_GLOBAL_BARRIER* pBarriers);

    void CreateVertexBuffer(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        UploadAllocation intermediate,
        ComPtr<ID3D12Resource>& vertexBuffer,
        D3D12_VERTEX_BUFFER_VIEW* pVertexBufferView,
        const std::vector<Vertex>& vertices);

    void CreateIndexBuffer(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        UploadAllocation intermediate,
        ComPtr<ID3D12Resource>& indexBuffer,
        D3D12_INDEX_BUFFER_VIEW* pindexBufferView,
        const std::vector<UINT32>& indices);

    void CreateSampler(
        ID3D12Device* pDevice,
        TextureFiltering filtering,
        TextureAddressingMode addressingMode,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

    UINT8 GetFormatPlaneCount(ID3D12Device* pDevice, DXGI_FORMAT format);

    UINT GetSubresourceCount(ID3D12Device* pDevice, ID3D12Resource* pResource);
    UINT GetSubresourceCount(ID3D12Device* pDevice, D3D12_RESOURCE_DESC desc);
    UINT GetSubresourceCount(ID3D12Device* pDevice, D3D12_RESOURCE_DESC1 desc);

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

    D3D12_CLEAR_VALUE CreateClearValue(DXGI_FORMAT format, float r, float g, float b, float a);
    D3D12_CLEAR_VALUE CreateClearValue(DXGI_FORMAT format, float depth, UINT8 stencil);
}