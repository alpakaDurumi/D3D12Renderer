#include "pch.h"
#include "D3DHelper.h"

#include "Utility.h"
#include "DirectXTex.h"

using namespace DirectX;

namespace D3DHelper
{
    void ThrowIfFailed(HRESULT hr)
    {
        if (FAILED(hr))
        {
            throw std::exception();
        }
    }

    // Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
    // If no such adapter can be found, *ppAdapter will be set to nullptr.
    void GetHardwareAdapter(
        _In_ IDXGIFactory1* pFactory,
        _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
        bool requestHighPerformanceAdapter)
    {
        *ppAdapter = nullptr;

        bool found = false;

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
                    // If you want a software adapter, pass in "-warp" or "--warp" on the command line.
                    continue;
                }

                // Check to see whether the adapter supports Direct3D 12, but don't create the
                // actual device yet.
                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    found = true;
                    break;
                }
            }
        }
        else
        {
            for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
            {
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                {
                    continue;
                }

                if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
                {
                    found = true;
                    break;
                }
            }
        }

        *ppAdapter = found ? adapter.Detach() : nullptr;
    }

    bool CheckTearingSupport()
    {
        BOOL allowTearing = FALSE;

        // Minimum DXGI version for Variable rate refresh is 1.5
        ComPtr<IDXGIFactory5> factory;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
        {
            if (FAILED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
            {
                allowTearing = FALSE;
            }
        }

        return allowTearing == TRUE;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(const D3D12_CPU_DESCRIPTOR_HANDLE& handle, INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE ret = handle;
        ret.ptr += SIZE_T(INT64(offsetInDescriptors) * INT64(descriptorIncrementSize));
        return ret;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(const D3D12_GPU_DESCRIPTOR_HANDLE& handle, INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE ret = handle;
        ret.ptr += UINT64(offsetInDescriptors) * UINT64(descriptorIncrementSize);
        return ret;
    }

    void DowngradeDescriptorRanges(const D3D12_DESCRIPTOR_RANGE1* src, UINT NumDescriptorRanges, std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges)
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

    void DowngradeRootDescriptor(const D3D12_ROOT_DESCRIPTOR1* src, D3D12_ROOT_DESCRIPTOR* dst)
    {
        dst->ShaderRegister = src->ShaderRegister;
        dst->RegisterSpace = src->RegisterSpace;
    }

    void DowngradeRootParameters(const D3D12_ROOT_PARAMETER1* src, UINT numParameters, D3D12_ROOT_PARAMETER* dst, std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges, UINT& offset)
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

    void CreateUploadBuffer(ID3D12Device10* pDevice, UINT64 requiredSize, ComPtr<ID3D12Resource>& uploadBuffer)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC1 resourceDesc = {};
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
        resourceDesc.SamplerFeedbackMipRegion = {};     // Not use Sampler Feedback

        ThrowIfFailed(pDevice->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_UNDEFINED,
            nullptr,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&uploadBuffer)));
    }

    // For vertex buffer and index buffer
    void CreateDefaultBuffer(ID3D12Device10* pDevice, UINT64 size, ComPtr<ID3D12Resource>& defaultBuffer)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC1 resourceDesc = {};
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
        resourceDesc.SamplerFeedbackMipRegion = {};     // Not use Sampler Feedback

        ThrowIfFailed(pDevice->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_UNDEFINED,
            nullptr,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&defaultBuffer)));
    }

    void CreateDefaultTexture(ID3D12Device10* pDevice, UINT64 width, UINT height, ComPtr<ID3D12Resource>& defaultTexture)
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC1 resourceDesc = {};
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
        resourceDesc.SamplerFeedbackMipRegion = {};     // Not use Sampler Feedback

        ThrowIfFailed(pDevice->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_COMMON,
            nullptr,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&defaultTexture)));
    }

    // If pClearValue is provided, rtvFormat parameter will be ignored.
    void CreateRenderTarget(ID3D12Device10* pDevice, UINT64 width, UINT height, DXGI_FORMAT format, DXGI_FORMAT rtvFormat, UINT16 depthOrArraySize, ComPtr<ID3D12Resource>& renderTarget, D3D12_CLEAR_VALUE* pClearValue)
    {
        auto defaultClearValue = CreateClearValue(rtvFormat, 0.0f, 0.0f, 0.0f, 0.0f);

        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        auto resourceDesc = GetRenderTargetDesc(width, height, depthOrArraySize, format);

        ThrowIfFailed(pDevice->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_RENDER_TARGET,
            pClearValue == nullptr ? &defaultClearValue : pClearValue,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&renderTarget)));
    }

    void CreateDepthStencilBuffer(ID3D12Device10* pDevice, UINT64 width, UINT height, UINT16 depthOrArraySize, ComPtr<ID3D12Resource>& depthStencilBuffer, bool useStencil, D3D12_CLEAR_VALUE* pClearValue)
    {
        // reverse-z
        auto defaultClearValue = CreateClearValue(useStencil ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT, 0.0f, 0);

        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        auto resourceDesc = GetDepthStencilBufferDesc(width, height, depthOrArraySize, useStencil);

        ThrowIfFailed(pDevice->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
            pClearValue == nullptr ? &defaultClearValue : pClearValue,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&depthStencilBuffer)));
    }

    void CreateRTV(ID3D12Device10* pDevice, ID3D12Resource* pResource, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, bool isArray, UINT firstArraySlice)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = format;

        if (isArray)
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.MipSlice = 0;
            rtvDesc.Texture2DArray.FirstArraySlice = firstArraySlice;
            rtvDesc.Texture2DArray.ArraySize = 1;
            rtvDesc.Texture2DArray.PlaneSlice = 0;
        }
        else
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;
        }

        pDevice->CreateRenderTargetView(pResource, &rtvDesc, cpuHandle);
    }

    void CreateDSV(ID3D12Device10* pDevice, ID3D12Resource* pResource, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, bool useStencil, bool isReadOnly, bool isArray, UINT firstArraySlice)
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = useStencil ? DXGI_FORMAT_D24_UNORM_S8_UINT : DXGI_FORMAT_D32_FLOAT;
        dsvDesc.Flags = isReadOnly ? D3D12_DSV_FLAG_READ_ONLY_DEPTH | D3D12_DSV_FLAG_READ_ONLY_STENCIL : D3D12_DSV_FLAG_NONE;

        if (isArray)
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.MipSlice = 0;
            dsvDesc.Texture2DArray.ArraySize = 1;
            dsvDesc.Texture2DArray.FirstArraySlice = firstArraySlice;
        }
        else
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
        }

        pDevice->CreateDepthStencilView(pResource, &dsvDesc, cpuHandle);
    }

    void CreateSRV(ID3D12Device10* pDevice, ID3D12Resource* pResource, DXGI_FORMAT format, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

        pDevice->CreateShaderResourceView(pResource, &srvDesc, cpuHandle);
    }

    void CreateCBV(ID3D12Device10* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr, UINT size, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = gpuPtr;
        cbvDesc.SizeInBytes = size;

        pDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);
    }

    D3D12_RESOURCE_DESC1 GetDepthStencilBufferDesc(UINT64 width, UINT height, UINT16 depthOrArraySize, bool useStencil)
    {
        D3D12_RESOURCE_DESC1 desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = depthOrArraySize;
        desc.MipLevels = 1;
        desc.Format = useStencil ? DXGI_FORMAT_R24G8_TYPELESS : DXGI_FORMAT_R32_TYPELESS;
        desc.SampleDesc = { 1, 0 };
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        desc.SamplerFeedbackMipRegion = {};     // Not use Sampler Feedback
        return desc;
    }

    D3D12_RESOURCE_DESC1 GetRenderTargetDesc(UINT64 width, UINT height, UINT16 depthOrArraySize, DXGI_FORMAT format)
    {
        D3D12_RESOURCE_DESC1 desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Alignment = 0;
        desc.Width = width;
        desc.Height = height;
        desc.DepthOrArraySize = depthOrArraySize;
        desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc = { 1, 0 };
        desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        desc.SamplerFeedbackMipRegion = {};     // Not use Sampler Feedback
        return desc;
    }

    // Assume that intermediate resource is already mapped
    void UpdateSubresources(
        ID3D12Device* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        ID3D12Resource* pDest,
        UploadAllocation intermediate,
        UINT firstSubresource,
        UINT numSubresources,
        D3D12_SUBRESOURCE_DATA* pSrcData)
    {
        // Calculate required size for data upload and allocate heap memory for footprints
        // Structure of Arrays
        UINT64 memToAlloc = static_cast<UINT64>(sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)) * numSubresources;
        void* pMem = HeapAlloc(GetProcessHeap(), 0, static_cast<SIZE_T>(memToAlloc));
        if (pMem == nullptr)
        {
            return;
        }

        // Acquire footprint of each subresource
        D3D12_RESOURCE_DESC desc = pDest->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = static_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
        UINT* pNumRows = reinterpret_cast<UINT*>(pLayouts + numSubresources);
        UINT64* pRowSizeInBytes = reinterpret_cast<UINT64*>(pNumRows + numSubresources);
        UINT64 requiredSize = 0;
        // 네 번째 인자인 BaseOffset은 출력되는 pLayouts[i].Offset들에 더해지는 값이다
        pDevice->GetCopyableFootprints(&desc, firstSubresource, numSubresources, 0, pLayouts, pNumRows, pRowSizeInBytes, &requiredSize);

        // dest의 레이아웃에 맞춰서 intermediate로 데이터를 복사
        // Each subresource
        for (UINT i = 0; i < numSubresources; i++)
        {
            //D3D12_MEMCPY_DEST DestData = { pData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch, SIZE_T(pLayouts[i].Footprint.RowPitch) * SIZE_T(pNumRows[i]) };
            auto pIntermediateStart = static_cast<UINT8*>(intermediate.CPUPtr) + pLayouts[i].Offset;
            auto rowPitch = pLayouts[i].Footprint.RowPitch;
            auto slicePitch = SIZE_T(pLayouts[i].Footprint.RowPitch) * SIZE_T(pNumRows[i]);
            // Each depth (slice)
            for (UINT z = 0; z < pLayouts[i].Footprint.Depth; ++z)
            {
                auto pIntermediateSlice = static_cast<UINT8*>(pIntermediateStart) + slicePitch * z;
                auto pSrcSlice = static_cast<const UINT8*>(pSrcData[i].pData) + pSrcData[i].SlicePitch * LONG_PTR(z);
                // Each Row
                for (UINT y = 0; y < pNumRows[i]; ++y)
                {
                    memcpy(pIntermediateSlice + rowPitch * y,
                        pSrcSlice + pSrcData[i].RowPitch * LONG_PTR(y),
                        pRowSizeInBytes[i]);
                }
            }
        }

        // Copy from upload heap to default heap
        // Buffer has only one subresource
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            pCommandList->CopyBufferRegion(pDest, 0, intermediate.pResource, intermediate.Offset + pLayouts[0].Offset, pLayouts[0].Footprint.Width);
        }
        // Texture has one or more subresources
        else
        {
            for (UINT i = 0; i < numSubresources; i++)
            {
                D3D12_TEXTURE_COPY_LOCATION dst = {};
                dst.pResource = pDest;
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = i + firstSubresource;

                D3D12_TEXTURE_COPY_LOCATION src = {};
                src.pResource = intermediate.pResource;
                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src.PlacedFootprint = pLayouts[i];
                src.PlacedFootprint.Offset += intermediate.Offset;

                pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
            }
        }

        HeapFree(GetProcessHeap(), 0, pMem);
    }

    // Legacy barrier. Not use
    D3D12_RESOURCE_BARRIER GetTransitionBarrier(ComPtr<ID3D12Resource>& resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
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

    D3D12_BARRIER_GROUP BufferBarrierGroup(UINT32 numBarriers, D3D12_BUFFER_BARRIER* pBarriers)
    {
        D3D12_BARRIER_GROUP group = {};
        group.Type = D3D12_BARRIER_TYPE_BUFFER;
        group.NumBarriers = numBarriers;
        group.pBufferBarriers = pBarriers;
        return group;
    }

    D3D12_BARRIER_GROUP TextureBarrierGroup(UINT32 numBarriers, D3D12_TEXTURE_BARRIER* pBarriers)
    {
        D3D12_BARRIER_GROUP group = {};
        group.Type = D3D12_BARRIER_TYPE_TEXTURE;
        group.NumBarriers = numBarriers;
        group.pTextureBarriers = pBarriers;
        return group;
    }

    D3D12_BARRIER_GROUP GlobalBarrierGroup(UINT32 numBarriers, D3D12_GLOBAL_BARRIER* pBarriers)
    {
        D3D12_BARRIER_GROUP group = {};
        group.Type = D3D12_BARRIER_TYPE_GLOBAL;
        group.NumBarriers = numBarriers;
        group.pGlobalBarriers = pBarriers;
        return group;
    }

    void CreateVertexBuffer(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        UploadAllocation intermediate,
        ComPtr<ID3D12Resource>& vertexBuffer,
        D3D12_VERTEX_BUFFER_VIEW* pVertexBufferView,
        const std::vector<Vertex>& vertices)
    {
        const UINT vertexBufferSize = UINT(vertices.size()) * UINT(sizeof(Vertex));

        CreateDefaultBuffer(pDevice, vertexBufferSize, vertexBuffer);

        D3D12_SUBRESOURCE_DATA vertexData = {};
        vertexData.pData = vertices.data();
        vertexData.RowPitch = vertexBufferSize;
        vertexData.SlicePitch = vertexData.RowPitch;

        UpdateSubresources(pDevice, pCommandList, vertexBuffer.Get(), intermediate, 0, 1, &vertexData);

        D3D12_BUFFER_BARRIER b = {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_VERTEX_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_VERTEX_BUFFER,
            vertexBuffer.Get(),
            0,
            UINT64_MAX
        };

        D3D12_BARRIER_GROUP barrierGroups[] = { BufferBarrierGroup(1, &b) };
        pCommandList->Barrier(1, barrierGroups);

        // Initialize the vertex buffer view
        pVertexBufferView->BufferLocation = vertexBuffer->GetGPUVirtualAddress();
        pVertexBufferView->StrideInBytes = static_cast<UINT>(sizeof(Vertex));
        pVertexBufferView->SizeInBytes = vertexBufferSize;
    }

    void CreateIndexBuffer(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        UploadAllocation intermediate,
        ComPtr<ID3D12Resource>& indexBuffer,
        D3D12_INDEX_BUFFER_VIEW* pindexBufferView,
        const std::vector<UINT32>& indices)
    {
        const UINT indexBufferSize = UINT(indices.size()) * UINT(sizeof(UINT32));

        CreateDefaultBuffer(pDevice, indexBufferSize, indexBuffer);

        D3D12_SUBRESOURCE_DATA indexData = {};
        indexData.pData = indices.data();
        indexData.RowPitch = indexBufferSize;
        indexData.SlicePitch = indexData.RowPitch;

        UpdateSubresources(pDevice, pCommandList, indexBuffer.Get(), intermediate, 0, 1, &indexData);

        D3D12_BUFFER_BARRIER b = {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_INDEX_INPUT,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_INDEX_BUFFER,
            indexBuffer.Get(),
            0,
            UINT64_MAX
        };

        D3D12_BARRIER_GROUP barrierGroups[] = { BufferBarrierGroup(1, &b) };
        pCommandList->Barrier(1, barrierGroups);

        // Initialize the index buffer view
        pindexBufferView->BufferLocation = indexBuffer->GetGPUVirtualAddress();
        pindexBufferView->SizeInBytes = indexBufferSize;
        pindexBufferView->Format = DXGI_FORMAT_R32_UINT;
    }

    void CreateSRVForShadow(
        ID3D12Device10* pDevice,
        ID3D12Resource* pResource,
        D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle,
        LightType type)
    {
        // Create only one SRV.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        switch (type)
        {
        case LightType::DIRECTIONAL:
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.ArraySize = MAX_CASCADES;
            srvDesc.Texture2DArray.PlaneSlice = 0;
            srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
            break;
        }
        case LightType::POINT:
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
            srvDesc.TextureCube.MostDetailedMip = 0;
            srvDesc.TextureCube.MipLevels = 1;
            srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
            break;
        }
        case LightType::SPOT:
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.PlaneSlice = 0;
            srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
            break;
        }
        }
        pDevice->CreateShaderResourceView(pResource, &srvDesc, srvCpuHandle);
    }

    void CreateSampler(
        ID3D12Device* pDevice,
        TextureFiltering filtering,
        TextureAddressingMode addressingMode,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
    {
        D3D12_SAMPLER_DESC samplerDesc = {};

        switch (filtering)
        {
        case TextureFiltering::POINT:
            samplerDesc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            samplerDesc.MaxAnisotropy = 0;
            break;
        case TextureFiltering::BILINEAR:
            samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            samplerDesc.MaxAnisotropy = 0;
            break;
        case TextureFiltering::ANISOTROPIC_X2:
            samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
            samplerDesc.MaxAnisotropy = 2;
            break;
        case TextureFiltering::ANISOTROPIC_X4:
            samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
            samplerDesc.MaxAnisotropy = 4;
            break;
        case TextureFiltering::ANISOTROPIC_X8:
            samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
            samplerDesc.MaxAnisotropy = 8;
            break;
        case TextureFiltering::ANISOTROPIC_X16:
            samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
            samplerDesc.MaxAnisotropy = 16;
            break;
        }

        switch (addressingMode)
        {
        case TextureAddressingMode::WRAP:
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            break;
        case TextureAddressingMode::MIRROR:
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
            break;
        case TextureAddressingMode::CLAMP:
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            break;
        case TextureAddressingMode::BORDER:
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            break;
        case TextureAddressingMode::MIRROR_ONCE:
            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
            break;
        }

        samplerDesc.MipLODBias = 0;
        samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NONE;
        samplerDesc.BorderColor[0] = 0.0f;
        samplerDesc.BorderColor[1] = 0.0f;
        samplerDesc.BorderColor[2] = 0.0f;
        samplerDesc.BorderColor[3] = 0.0f;
        samplerDesc.MinLOD = 0.0f;
        samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

        pDevice->CreateSampler(&samplerDesc, cpuHandle);
    }

    UINT16 GetRequiredArraySize(LightType type)
    {
        switch (type)
        {
        case LightType::DIRECTIONAL:
            return MAX_CASCADES;
        case LightType::POINT:
            return POINT_LIGHT_ARRAY_SIZE;
        case LightType::SPOT:
            return SPOT_LIGHT_ARRAY_SIZE;
        default:
            return -1;
        }
    }

    UINT8 GetFormatPlaneCount(ID3D12Device* pDevice, DXGI_FORMAT format)
    {
        D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = { format };
        if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo))))
        {
            return 0;
        }
        return formatInfo.PlaneCount;
    }

    UINT GetSubresourceCount(ID3D12Device* pDevice, ID3D12Resource* pResource)
    {
        auto desc = pResource->GetDesc();
        return GetSubresourceCount(pDevice, desc);
    }

    UINT GetSubresourceCount(ID3D12Device* pDevice, D3D12_RESOURCE_DESC desc)
    {
        UINT ret = desc.MipLevels * GetFormatPlaneCount(pDevice, desc.Format);
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D) ret *= desc.DepthOrArraySize;
        return ret;
    }

    UINT GetSubresourceCount(ID3D12Device* pDevice, D3D12_RESOURCE_DESC1 desc)
    {
        UINT ret = desc.MipLevels * GetFormatPlaneCount(pDevice, desc.Format);
        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D) ret *= desc.DepthOrArraySize;
        return ret;
    }

    UINT CalcSubresourceIndex(
        UINT mipIndex,
        UINT arrayIndex,
        UINT planeIndex,
        UINT mipLevels,
        UINT arraySize)
    {
        return mipIndex + arrayIndex * mipLevels + planeIndex * mipLevels * arraySize;
    }

    void ConvertToDDS(
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage)
    {
        bool isHDR = false;

        std::wstring ext = Utility::GetFileExtension(filePath);

        // Load texture based on file extension
        // No exr extension considered yet
        TexMetadata info;
        ScratchImage image;

        if (ext == L"dds")
        {
            HRESULT hr = LoadFromDDSFile(filePath.c_str(), DDS_FLAGS_NONE, &info, image);
            if (FAILED(hr))
            {
                throw std::runtime_error("Could not load DDS texture.");
            }
        }
        else if (ext == L"tga")
        {
            HRESULT hr = LoadFromTGAFile(filePath.c_str(), &info, image);
            if (FAILED(hr))
            {
                throw std::runtime_error("Could not load TGA texture.");
            }
        }
        else if (ext == L"hdr")
        {
            isHDR = true;
            HRESULT hr = LoadFromHDRFile(filePath.c_str(), &info, image);
            if (FAILED(hr))
            {
                throw std::runtime_error("Could not load HDR texture.");
            }
        }
        else
        {
            // Setting SRGB flag does not change image data. It only affects format of metadata.
            HRESULT hr = LoadFromWICFile(filePath.c_str(), isSRGB ? WIC_FLAGS_DEFAULT_SRGB : WIC_FLAGS_NONE, &info, image);
            if (FAILED(hr))
            {
                throw std::runtime_error("Could not load WIC texture.");
            }
        }

        // Check resource limits
        if (info.dimension == TEX_DIMENSION_TEXTURE1D)
        {
            if (info.width > D3D12_REQ_TEXTURE1D_U_DIMENSION)
            {
                throw std::runtime_error("Texture1D size is too large.");
            }
        }
        else if (info.dimension == TEX_DIMENSION_TEXTURE2D)
        {
            if (info.width > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
                info.height > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
            {
                throw std::runtime_error("Texture2D size is too large.");
            }
        }
        else
        {
            if (info.width > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION ||
                info.height > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION ||
                info.depth > D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION)
            {
                throw std::runtime_error("Texture3D size is too large.");
            }
        }

        // Check size and set target BC format
        DXGI_FORMAT targetBCFormat;
        if (useBlockCompress)
        {
            if (info.width % 4 || info.height % 4)
            {
                throw std::runtime_error("BC compressed image width and height should be multiple of 4.");
            }

            if (isHDR)
            {
                targetBCFormat = DXGI_FORMAT_BC6H_UF16;
            }
            else
            {
                targetBCFormat = isSRGB ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM;
            }
        }

        // If image already compressed, decompress it on demand.
        // FlipRotate and GenerateMipMaps do not operate directly on block compressed images.
        if (IsCompressed(info.format))
        {
            if (!useBlockCompress || info.format != targetBCFormat || flipImage || info.mipLevels == 1)
            {
                DXGI_FORMAT targetDecompressedFormat = DXGI_FORMAT_UNKNOWN;     // Pick default format based on the input BC format

                ScratchImage decompressed;

                HRESULT hr = Decompress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), targetDecompressedFormat, decompressed);

                if (FAILED(hr))
                {
                    throw std::runtime_error("Failed to decompressing image.");
                }
                else
                {
                    image = std::move(decompressed);
                }
            }
        }

        // Flip image
        // Only flips first subresource only
        if (flipImage)
        {
            ScratchImage flipped;

            HRESULT hr = FlipRotate(image.GetImages()[0], TEX_FR_FLIP_VERTICAL, flipped);

            if (FAILED(hr))
            {
                throw std::runtime_error("Failed to flipping image.");
            }
            else
            {
                image = std::move(flipped);
            }
        }

        // Generate mipmaps
        // If image flipped, mipmaps should be regenerated.
        // Automatically handles SRGB <-> linear conversion based on metadata of image.
        if (info.mipLevels == 1 || flipImage)
        {
            ScratchImage mipChain;

            HRESULT hr = GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), TEX_FILTER_DEFAULT, 0, mipChain);
            if (FAILED(hr))
            {
                throw std::runtime_error("Failed to generating mipmaps.");
            }
            else
            {
                image = std::move(mipChain);
            }
        }

        // Compress image
        if (useBlockCompress && info.format != targetBCFormat)
        {
            ScratchImage compressed;

            // CPU codec Compressing. GPU compression is not yet supported for d3d12 devices.
            HRESULT hr = Compress(image.GetImages(), image.GetImageCount(), image.GetMetadata(), targetBCFormat, TEX_COMPRESS_BC7_QUICK | TEX_COMPRESS_PARALLEL, TEX_THRESHOLD_DEFAULT, compressed);
            if (FAILED(hr))
            {
                throw std::runtime_error("Failed to compressing images.");
            }
            else
            {
                image = std::move(compressed);
            }
        }

        // Save DDS file
        const std::wstring resultName = Utility::RemoveFileExtension(filePath) + L".dds";
        HRESULT hr = SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DDS_FLAGS_NONE, resultName.c_str());
        if (FAILED(hr))
        {
            throw std::runtime_error("Failed to save dds file.");
        }
    }

    D3D12_CLEAR_VALUE CreateClearValue(DXGI_FORMAT format, float r, float g, float b, float a)
    {
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = format;
        clearValue.Color[0] = r;
        clearValue.Color[1] = g;
        clearValue.Color[2] = b;
        clearValue.Color[3] = a;
        return clearValue;
    }

    D3D12_CLEAR_VALUE CreateClearValue(DXGI_FORMAT format, float depth, UINT8 stencil)
    {
        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = format;
        clearValue.DepthStencil.Depth = depth;
        clearValue.DepthStencil.Stencil = stencil;
        return clearValue;
    }
}