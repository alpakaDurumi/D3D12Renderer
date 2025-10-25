#include "D3DHelper.h"

#include <exception>
#include <cassert>

#include "ResourceLayoutTracker.h"

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
                    break;
                }
            }
        }

        // For backward compatibility
        if (adapter.Get() == nullptr)
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
                    break;
                }
            }
        }

        *ppAdapter = adapter.Detach();
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

    void MoveCPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* handle, INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        handle->ptr = SIZE_T(INT64(handle->ptr) + INT64(offsetInDescriptors) * INT64(descriptorIncrementSize));
    }

    void MoveGPUDescriptorHandle(D3D12_GPU_DESCRIPTOR_HANDLE* handle, INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        handle->ptr = handle->ptr + UINT64(offsetInDescriptors) * UINT64(descriptorIncrementSize);
    }

    void MoveCPUAndGPUDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle,
        INT offsetInDescriptors, INT descriptorIncrementSize)
    {
        MoveCPUDescriptorHandle(cpuHandle, offsetInDescriptors, descriptorIncrementSize);
        MoveGPUDescriptorHandle(gpuHandle, offsetInDescriptors, descriptorIncrementSize);
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

    void CreateUploadHeap(ComPtr<ID3D12Device10>& device, UINT64 requiredSize, ComPtr<ID3D12Resource>& uploadHeap)
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

        ThrowIfFailed(device->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_UNDEFINED,
            nullptr,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&uploadHeap)));
    }

    // For vertex buffer and index buffer
    void CreateDefaultHeapForBuffer(ComPtr<ID3D12Device10>& device, UINT64 size, ComPtr<ID3D12Resource>& defaultHeap)
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

        ThrowIfFailed(device->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_UNDEFINED,
            nullptr,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&defaultHeap)));
    }

    void CreateDefaultHeapForTexture(ComPtr<ID3D12Device10>& device, ComPtr<ID3D12Resource>& defaultHeap, UINT width, UINT height)
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

        ThrowIfFailed(device->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_COMMON,
            nullptr,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&defaultHeap)));
    }

    void UpdateSubresources(
        ComPtr<ID3D12Device10>& device,
        ComPtr<ID3D12GraphicsCommandList7>& commandList,
        ComPtr<ID3D12Resource>& dest,
        ComPtr<ID3D12Resource>& intermediate,
        UINT64 intermediateOffset,
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
        D3D12_RESOURCE_DESC desc = dest->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts = static_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
        UINT* pNumRows = reinterpret_cast<UINT*>(pLayouts + numSubresources);
        UINT64* pRowSizeInBytes = reinterpret_cast<UINT64*>(pNumRows + numSubresources);
        UINT64 requiredSize = 0;
        device->GetCopyableFootprints(&desc, firstSubresource, numSubresources, intermediateOffset, pLayouts, pNumRows, pRowSizeInBytes, &requiredSize);

        // Map intermediate resource (upload heap)
        D3D12_RANGE readRange = { 0, 0 };   // do not read from CPU. only write
        UINT8* pData;
        ThrowIfFailed(intermediate->Map(0, &readRange, reinterpret_cast<void**>(&pData)));

        // dest의 레이아웃에 맞춰서 intermediate로 데이터를 복사
        // Each subresource
        for (UINT i = 0; i < numSubresources; i++)
        {
            //D3D12_MEMCPY_DEST DestData = { pData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch, SIZE_T(pLayouts[i].Footprint.RowPitch) * SIZE_T(pNumRows[i]) };
            auto pIntermediateStart = pData + pLayouts[i].Offset;
            auto rowPitch = pLayouts[i].Footprint.RowPitch;
            auto slicePitch = SIZE_T(pLayouts[i].Footprint.RowPitch) * SIZE_T(pNumRows[i]);
            // Each depth (slice)
            for (UINT z = 0; z < pLayouts[i].Footprint.Depth; ++z)
            {
                auto pIntermediateSlice = static_cast<BYTE*>(pIntermediateStart) + slicePitch * z;
                auto pSrcSlice = static_cast<const BYTE*>(pSrcData[i].pData) + pSrcData[i].SlicePitch * LONG_PTR(z);
                // Each Row
                for (UINT y = 0; y < pNumRows[i]; ++y)
                {
                    memcpy(pIntermediateSlice + rowPitch * y,
                        pSrcSlice + pSrcData[i].RowPitch * LONG_PTR(y),
                        pRowSizeInBytes[i]);
                }
            }
        }

        // Unmap
        intermediate->Unmap(0, nullptr);

        // Copy from upload heap to default heap
        // Buffer has only one subresource
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
        {
            commandList->CopyBufferRegion(dest.Get(), 0, intermediate.Get(), pLayouts[0].Offset, pLayouts[0].Footprint.Width);
        }
        // Texture has one or more subresources
        else
        {
            for (UINT i = 0; i < numSubresources; i++)
            {
                D3D12_TEXTURE_COPY_LOCATION dst = {};
                dst.pResource = dest.Get();
                dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dst.SubresourceIndex = i + firstSubresource;

                D3D12_TEXTURE_COPY_LOCATION src = {};
                src.pResource = intermediate.Get();
                src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
                src.PlacedFootprint = pLayouts[i];

                commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
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

    void Barrier(
        ID3D12GraphicsCommandList7* pCommandList,
        ID3D12Resource* pResource,
        D3D12_BARRIER_SYNC syncBefore,
        D3D12_BARRIER_SYNC syncAfter,
        D3D12_BARRIER_ACCESS accessBefore,
        D3D12_BARRIER_ACCESS accessAfter)
    {
        D3D12_RESOURCE_DESC desc = pResource->GetDesc();
        assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER);

        D3D12_BUFFER_BARRIER barrier =
        {
            syncBefore,
            syncAfter,
            accessBefore,
            accessAfter,
            pResource,
            0,
            UINT64_MAX
        };
        D3D12_BARRIER_GROUP barrierGroups[] = { BufferBarrierGroup(1, &barrier) };
        pCommandList->Barrier(1, barrierGroups);
    }

    void Barrier(
        ID3D12GraphicsCommandList7* pCommandList,
        ID3D12Resource* pResource,
        ResourceLayoutTracker& layoutTracker,
        D3D12_BARRIER_SYNC syncBefore,
        D3D12_BARRIER_SYNC syncAfter,
        D3D12_BARRIER_ACCESS accessBefore,
        D3D12_BARRIER_ACCESS accessAfter,
        D3D12_BARRIER_LAYOUT layoutAfter,
        D3D12_BARRIER_SUBRESOURCE_RANGE subresourceRange)
    {
        D3D12_RESOURCE_DESC desc = pResource->GetDesc();
        assert(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ||
            desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
            desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D);

        // subresourceRange를 순회하며 레이아웃을 확인하여 배열에 추가한 후, 배리어 그룹을 구성

        const auto& [IndexOrFirstMipLevel, NumMipLevels, FirstArraySlice, NumArraySlices, FirstPlane, NumPlanes] = subresourceRange;

        UINT count = NumMipLevels * NumArraySlices * NumPlanes;

        std::vector<D3D12_TEXTURE_BARRIER> barriers(count);

        // Target all subresources
        if (IndexOrFirstMipLevel == 0xffffffff)
        {
            for (UINT i = 0; i < count; i++)
            {
                D3D12_BARRIER_LAYOUT layoutBefore = layoutTracker.SetLayout(pResource, i, layoutAfter);

                barriers[i] =
                {
                    syncBefore,
                    syncAfter,
                    accessBefore,
                    accessAfter,
                    layoutBefore,
                    layoutAfter,
                    pResource,
                    {i, 0, 0, 0, 0, 0},
                    D3D12_TEXTURE_BARRIER_FLAG_NONE
                };
            }
        }
        else
        {
            UINT idx = 0;
            for (UINT plane = FirstPlane; plane < FirstPlane + NumPlanes; ++plane)
            {
                for (UINT array = FirstArraySlice; array < FirstArraySlice + NumArraySlices; ++array)
                {
                    for (UINT mip = IndexOrFirstMipLevel; mip < IndexOrFirstMipLevel + NumMipLevels; ++mip)
                    {
                        auto [layoutBefore, subresourceIndex] = layoutTracker.SetLayout(pResource, mip, array, plane, layoutAfter);

                        barriers[idx] =
                        {
                            syncBefore,
                            syncAfter,
                            accessBefore,
                            accessAfter,
                            layoutBefore,
                            layoutAfter,
                            pResource,
                            {subresourceIndex, 0, 0, 0, 0, 0},
                            D3D12_TEXTURE_BARRIER_FLAG_NONE
                        };

                        idx++;
                    }
                }
            }
        }

        D3D12_BARRIER_GROUP barrierGroups[] = { TextureBarrierGroup(count, barriers.data()) };
        pCommandList->Barrier(1, barrierGroups);
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

    void CreateIndexBuffer(
        ComPtr<ID3D12Device10>& device,
        ComPtr<ID3D12GraphicsCommandList7>& commandList,
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

        UpdateSubresources(device, commandList, indexBuffer, uploadHeap, 0, 0, 1, &indexData);

        D3D12_BUFFER_BARRIER barrier =
        {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_INDEX_INPUT,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_INDEX_BUFFER,
            indexBuffer.Get(),
            0,
            indexBufferSize
        };
        D3D12_BARRIER_GROUP barrierGroups[] = { BufferBarrierGroup(1, &barrier) };
        commandList->Barrier(1, barrierGroups);

        // Initialize the index buffer view
        pindexBufferView->BufferLocation = indexBuffer->GetGPUVirtualAddress();
        pindexBufferView->SizeInBytes = indexBufferSize;
        pindexBufferView->Format = DXGI_FORMAT_R32_UINT;
    }

    void CreateTexture(
        ComPtr<ID3D12Device10>& device,
        ComPtr<ID3D12GraphicsCommandList7>& commandList,
        ComPtr<ID3D12Resource>& texture,
        ComPtr<ID3D12Resource>& uploadHeap,
        std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
    {
        CreateDefaultHeapForTexture(device, texture, width, height);

        D3D12_TEXTURE_BARRIER barrier =
        {
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_LAYOUT_COMMON,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            texture.Get(),
            {0xffffffff, 0, 0, 0, 0, 0},    // Select all subresources
            D3D12_TEXTURE_BARRIER_FLAG_NONE
        };
        D3D12_BARRIER_GROUP barrierGroups[] = { TextureBarrierGroup(1, &barrier) };
        commandList->Barrier(1, barrierGroups);

        // Calculate required size for data upload
        D3D12_RESOURCE_DESC desc = texture->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 requiredSize = 0;
        device->GetCopyableFootprints(&desc, 0, 1, 0, &layouts, &numRows, &rowSizeInBytes, &requiredSize);

        CreateUploadHeap(device, requiredSize, uploadHeap);

        // 텍스처 데이터는 인자로 받도록 수정하기
        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = textureSrc.data();
        textureData.RowPitch = width * 4;   // 4 bytes per pixel (RGBA)
        textureData.SlicePitch = textureData.RowPitch * height;

        UpdateSubresources(device, commandList, texture, uploadHeap, 0, 0, 1, &textureData);

        barrier =
        {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
            texture.Get(),
            {0xffffffff, 0, 0, 0, 0, 0},    // Select all subresources
            D3D12_TEXTURE_BARRIER_FLAG_NONE
        };
        barrierGroups[0] = TextureBarrierGroup(1, &barrier);
        commandList->Barrier(1, barrierGroups);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        device->CreateShaderResourceView(texture.Get(), &srvDesc, cpuHandle);
    }

    void CreateDepthStencilBuffer(
        ComPtr<ID3D12Device10>& device,
        UINT width,
        UINT height,
        ComPtr<ID3D12Resource>& depthStencilBuffer,
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
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
        resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
        resourceDesc.SampleDesc = { 1, 0 };
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        resourceDesc.SamplerFeedbackMipRegion = {};     // Not use Sampler Feedback

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        ThrowIfFailed(device->CreateCommittedResource3(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
            &depthOptimizedClearValue,
            nullptr,
            0,
            nullptr,
            IID_PPV_ARGS(&depthStencilBuffer)));

        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
        depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;

        device->CreateDepthStencilView(depthStencilBuffer.Get(), &depthStencilViewDesc, cpuHandle);
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

    UINT CalcSubresourceIndex(
        UINT mipIndex,
        UINT arrayIndex,
        UINT planeIndex,
        UINT mipLevels,
        UINT arraySize)
    {
        return mipIndex + arrayIndex * mipLevels + planeIndex * mipLevels * arraySize;
    }
}