#pragma once

#include <Windows.h>
#include <exception>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

namespace D3DHelper
{
    inline void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw std::exception();
        }
    }

    // Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
    // If no such adapter can be found, *ppAdapter will be set to nullptr.
    inline void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter = true)
    {
        *ppAdapter = nullptr;

        ComPtr<IDXGIAdapter1> adapter;

        ComPtr<IDXGIFactory6> factory6;
        if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
        {
            for (
                UINT adapterIndex = 0;
                SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                    adapterIndex,
                    requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                    IID_PPV_ARGS(&adapter)));
                    ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    // If you want a software adapter, pass in "/warp" on the command line.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        if (adapter.Get() == nullptr)
        {
            for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    // Don't select the Basic Render Driver adapter.
                    // If you want a software adapter, pass in "/warp" on the command line.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    break;
                }
            }
        }

        *ppAdapter = adapter.Detach();
    }

    inline void MoveCPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* handle, INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        handle->ptr = SIZE_T(INT64(handle->ptr) + INT64(offsetInDescriptors) * INT64(descriptorIncrementSize));
    }

    inline void MoveGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE* handle, INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        handle->ptr = handle->ptr + UINT64(offsetInDescriptors) * UINT64(descriptorIncrementSize);
    }

    inline void MoveCPUAndGPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle,
        INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        MoveCPUDescriptorHandle(cpuHandle, offsetInDescriptors, descriptorIncrementSize);
        MoveGPUDescriptorHandle(gpuHandle, offsetInDescriptors, descriptorIncrementSize);
    }

    inline void DowngradeDescriptorRanges(const D3D12_DESCRIPTOR_RANGE1* src, UINT NumDescriptorRanges,
        std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges)
    {
        D3D12_DESCRIPTOR_RANGE tempRange = {};
        for (UINT i = 0; i < NumDescriptorRanges; i++)
        {
            tempRange.RangeType = src[i].RangeType;
            tempRange.NumDescriptors = src[i].NumDescriptors;
            tempRange.BaseShaderRegister = src[i].BaseShaderRegister;
            tempRange.RegisterSpace = src[i].RegisterSpace;
            tempRange.OffsetInDescriptorsFromTableStart = src[i].OffsetInDescriptorsFromTableStart;
            convertedRanges.push_back(tempRange);
        }
    }

    inline void DowngradeRootDescriptor(D3D12_ROOT_DESCRIPTOR1* src, D3D12_ROOT_DESCRIPTOR* dst)
    {
        dst->ShaderRegister = src->ShaderRegister;
        dst->RegisterSpace = src->RegisterSpace;
    }

    inline void DowngradeRootParameters(D3D12_ROOT_PARAMETER1* src, UINT numParameters, D3D12_ROOT_PARAMETER* dst,
        std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges, UINT& offset)
    {
        for (UINT i = 0; i < numParameters; i++)
        {
            dst[i].ParameterType = src[i].ParameterType;

            const D3D12_ROOT_PARAMETER_TYPE& type = src[i].ParameterType;
            switch (type)
            {
            case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
            {
                const UINT NumDescriptorRanges = src[i].DescriptorTable.NumDescriptorRanges;
                DowngradeDescriptorRanges(src[i].DescriptorTable.pDescriptorRanges, NumDescriptorRanges, convertedRanges);
                dst[i].DescriptorTable = { NumDescriptorRanges, convertedRanges.data() + offset };
                offset += NumDescriptorRanges;
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
            {
                dst[i].Constants = src[i].Constants;
                break;
            }
            case D3D12_ROOT_PARAMETER_TYPE_CBV:
            case D3D12_ROOT_PARAMETER_TYPE_SRV:
            case D3D12_ROOT_PARAMETER_TYPE_UAV:
            {
                DowngradeRootDescriptor(&src[i].Descriptor, &dst[i].Descriptor);
                break;
            }
            }

            dst[i].ShaderVisibility = src[i].ShaderVisibility;
        }
    }

    inline void CreateUploadHeap(ComPtr<ID3D12Device>& device, UINT64 requiredSize, ComPtr<ID3D12Resource>& uploadHeap)
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
        resourceDesc.Width = requiredSize;
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
            IID_PPV_ARGS(&uploadHeap)));
    }

    // For vertex buffer and index buffer
    inline void CreateDefaultHeapForBuffer(ComPtr<ID3D12Device>& device, UINT64 size, ComPtr<ID3D12Resource>& defaultHeap)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = size;
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
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&defaultHeap)));
    }

    inline void CreateDefaultHeapForTexture(ComPtr<ID3D12Device>& device, ComPtr<ID3D12Resource>& defaultHeap, UINT width, UINT height)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = width;
        resourceDesc.Height = height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        resourceDesc.SampleDesc = { 1, 0 };
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed(device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&defaultHeap)));
    }

    inline void UpdateSubResources(
        ComPtr<ID3D12Device>& device,
        ComPtr<ID3D12GraphicsCommandList> commandList,
        ComPtr<ID3D12Resource>& dest,
        ComPtr<ID3D12Resource>& intermediate,
        D3D12_SUBRESOURCE_DATA* pSrcData)
    {
        // Calculate required size for data upload
        // 업데이트를 자주 한다면 이러한 정보를 객체에 멤버로 유지할 수도 있을 것 같다
        D3D12_RESOURCE_DESC desc = dest->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 requiredSize = 0;
        // 서브리소스가 1개이면서 offset도 0이라고 가정
        device->GetCopyableFootprints(&desc, 0, 1, 0, &layouts, &numRows, &rowSizeInBytes, &requiredSize);

        // dest의 레이아웃에 맞춰서 intermediate로 데이터를 복사
        D3D12_RANGE readRange = { 0, 0 };   // do not read from CPU. only write
        UINT8* pData;
        ThrowIfFailed(intermediate->Map(0, &readRange, reinterpret_cast<void**>(&pData)));
        for (UINT z = 0; z < layouts.Footprint.Depth; ++z)
        {
            auto pDestSlice = static_cast<BYTE*>(pData + layouts.Offset) + SIZE_T(layouts.Footprint.RowPitch) * SIZE_T(numRows) * z;
            auto pSrcSlice = static_cast<const BYTE*>(pSrcData->pData) + pSrcData->SlicePitch * LONG_PTR(z);
            for (UINT y = 0; y < numRows; ++y)
            {
                memcpy(pDestSlice + layouts.Footprint.RowPitch * y,
                    pSrcSlice + pSrcData->RowPitch * LONG_PTR(y),
                    rowSizeInBytes);
            }
        }
        intermediate->Unmap(0, nullptr);

        // Copy from upload heap to default heap
        // buffer
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            commandList->CopyBufferRegion(dest.Get(), 0, intermediate.Get(), layouts.Offset, layouts.Footprint.Width);
        }
        // texture
        else
        {
            D3D12_TEXTURE_COPY_LOCATION dst = {};
            dst.pResource = dest.Get();
            dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst.SubresourceIndex = 0;

            D3D12_TEXTURE_COPY_LOCATION src = {};
            src.pResource = intermediate.Get();
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = layouts;

            commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
    }

    inline D3D12_RESOURCE_BARRIER GetTransitionBarrier(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
    {
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        return barrier;
    }

    template<typename T>
    inline static void CreateVertexBuffer(
        ComPtr<ID3D12Device>& device,
        ComPtr<ID3D12GraphicsCommandList>& commandList,
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

        UpdateSubResources(device, commandList, vertexBuffer, uploadHeap, &vertexData);

        // Change resource state
        D3D12_RESOURCE_BARRIER barrier = GetTransitionBarrier(vertexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        commandList->ResourceBarrier(1, &barrier);

        // Initialize the vertex buffer view
        pvertexBufferView->BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        pvertexBufferView->StrideInBytes = sizeof(T);
        pvertexBufferView->SizeInBytes = vertexBufferSize;
    }

    inline static void CreateIndexBuffer(
        ComPtr<ID3D12Device>& device,
        ComPtr<ID3D12GraphicsCommandList>& commandList,
        ComPtr<ID3D12Resource>& indexBuffer,
        ComPtr<ID3D12Resource>& uploadHeap,
        D3D12_INDEX_BUFFER_VIEW* pindexBufferView,
        std::vector<UINT32>& indices)
    {
        const UINT indexBufferSize = UINT(indices.size()) * UINT(sizeof(UINT32));

        CreateDefaultHeapForBuffer(device, indexBufferSize, indexBuffer);

        CreateUploadHeap(device, indexBufferSize, uploadHeap);

        D3D12_SUBRESOURCE_DATA indexData = {};
        indexData.pData = indices.data();
        indexData.RowPitch = indexBufferSize;
        indexData.SlicePitch = indexData.RowPitch;

        UpdateSubResources(device, commandList, indexBuffer, uploadHeap, &indexData);

        // Change resource state
        D3D12_RESOURCE_BARRIER barrier = GetTransitionBarrier(indexBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commandList->ResourceBarrier(1, &barrier);

        // Initialize the index buffer view
        pindexBufferView->BufferLocation = indexBuffer->GetGPUVirtualAddress();
        pindexBufferView->SizeInBytes = indexBufferSize;
        pindexBufferView->Format = DXGI_FORMAT_R32_UINT;
    }
}