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

    void CreateUploadBuffer(ID3D12Device10* pDevice, UINT64 requiredSize, ComPtr<ID3D12Resource>& uploadBuffer);
    void CreateDefaultBuffer(ID3D12Device10* pDevice, UINT64 size, ComPtr<ID3D12Resource>& defaultBuffer);
    void CreateDefaultTexture(ID3D12Device10* pDevice, UINT64 width, UINT height, ComPtr<ID3D12Resource>& defaultTexture);
    void CreateRenderTarget(ID3D12Device10* pDevice, UINT64 width, UINT height, DXGI_FORMAT format, DXGI_FORMAT rtvFormat, UINT16 depthOrArraySize, ComPtr<ID3D12Resource>& renderTarget, D3D12_CLEAR_VALUE* pClearValue = nullptr);
    void CreateDepthStencilBuffer(ID3D12Device10* pDevice, UINT64 width, UINT height, UINT16 depthOrArraySize, ComPtr<ID3D12Resource>& depthStencilBuffer, bool useStencil, D3D12_CLEAR_VALUE* pClearValue = nullptr);

    void CreateRTV(ID3D12Device10* pDevice, ID3D12Resource* pResource, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, bool isArray = false, UINT firstArraySlice = 0);
    void CreateDSV(ID3D12Device10* pDevice, ID3D12Resource* pResource, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, bool useStencil, bool isReadOnly, bool isArray = false, UINT firstArraySlice = 0);
    void CreateSRV(ID3D12Device10* pDevice, ID3D12Resource* pResource, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, UINT planeSlice = 0);
    void CreateCBV(ID3D12Device10* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr, UINT size, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle);

    D3D12_RESOURCE_DESC1 GetDepthStencilBufferDesc(UINT64 width, UINT height, UINT16 depthOrArraySize, bool useStencil);
    D3D12_RESOURCE_DESC1 GetRenderTargetDesc(UINT64 width, UINT height, UINT16 depthOrArraySize, DXGI_FORMAT format);

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