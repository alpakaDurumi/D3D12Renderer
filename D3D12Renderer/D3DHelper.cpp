#include "pch.h"

#include "D3DHelper.h"

#include <cstring>

#include <winerror.h>

#include <DirectXTex.h>

#include "Utility.h"

using Microsoft::WRL::ComPtr;
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

D3D12_CPU_DESCRIPTOR_HANDLE GetCpuDescriptorHandle(const D3D12_CPU_DESCRIPTOR_HANDLE& handle, INT offsetInDescriptors, INT descriptorIncrementSize)
{
    D3D12_CPU_DESCRIPTOR_HANDLE ret = handle;
    ret.ptr += SIZE_T(INT64(offsetInDescriptors) * INT64(descriptorIncrementSize));
    return ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetGpuDescriptorHandle(const D3D12_GPU_DESCRIPTOR_HANDLE& handle, INT offsetInDescriptors, INT descriptorIncrementSize)
{
    D3D12_GPU_DESCRIPTOR_HANDLE ret = handle;
    ret.ptr += UINT64(offsetInDescriptors) * UINT64(descriptorIncrementSize);
    return ret;
}

void DowngradeDescriptorRanges(const D3D12_DESCRIPTOR_RANGE1* src, UINT NumDescriptorRanges, D3D12_DESCRIPTOR_RANGE* dst)
{
    for (UINT i = 0; i < NumDescriptorRanges; i++)
    {
        dst[i].RangeType = src[i].RangeType;
        dst[i].NumDescriptors = src[i].NumDescriptors;
        dst[i].BaseShaderRegister = src[i].BaseShaderRegister;
        dst[i].RegisterSpace = src[i].RegisterSpace;
        dst[i].OffsetInDescriptorsFromTableStart = src[i].OffsetInDescriptorsFromTableStart;
    }
}

void DowngradeRootDescriptor(const D3D12_ROOT_DESCRIPTOR1* src, D3D12_ROOT_DESCRIPTOR* dst)
{
    dst->ShaderRegister = src->ShaderRegister;
    dst->RegisterSpace = src->RegisterSpace;
}

void DowngradeRootParameters(const D3D12_ROOT_PARAMETER1* src, UINT numParameters, D3D12_ROOT_PARAMETER* dst, std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges)
{
    UINT offset = 0;
    for (UINT i = 0; i < numParameters; i++)
    {
        dst[i].ParameterType = src[i].ParameterType;

        const D3D12_ROOT_PARAMETER_TYPE& type = src[i].ParameterType;
        switch (type)
        {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        {
            const UINT NumDescriptorRanges = src[i].DescriptorTable.NumDescriptorRanges;
            DowngradeDescriptorRanges(src[i].DescriptorTable.pDescriptorRanges, NumDescriptorRanges, convertedRanges.data() + offset);
            dst[i].DescriptorTable = {NumDescriptorRanges, convertedRanges.data() + offset};
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

D3D12_RESOURCE_DESC1 GetTexture2DDesc(UINT64 width, UINT height, UINT16 arraySize, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
{
    D3D12_RESOURCE_DESC1 desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = arraySize;
    desc.MipLevels = mipLevels;
    desc.Format = format;
    desc.SampleDesc = {1, 0};
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;
    desc.SamplerFeedbackMipRegion = {};

    return desc;
}

D3D12_RESOURCE_DESC1 GetTexture3DDesc(UINT64 width, UINT height, UINT16 depth, UINT16 mipLevels, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags)
{
    D3D12_RESOURCE_DESC1 desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
    desc.Alignment = 0;
    desc.Width = width;
    desc.Height = height;
    desc.DepthOrArraySize = depth;
    desc.MipLevels = mipLevels;
    desc.Format = format;
    desc.SampleDesc = {1, 0};
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;
    desc.SamplerFeedbackMipRegion = {};

    return desc;
}

D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc(DXGI_FORMAT format, UINT mipSlice, UINT planeSlice)
{
    D3D12_RENDER_TARGET_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    desc.Texture2D.MipSlice = mipSlice;
    desc.Texture2D.PlaneSlice = planeSlice;

    return desc;
}

D3D12_RENDER_TARGET_VIEW_DESC GetRtvDesc2DArray(DXGI_FORMAT format, UINT mipSlice, UINT firstArraySlice, UINT arraySize, UINT planeSlice)
{
    D3D12_RENDER_TARGET_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    desc.Texture2DArray.MipSlice = mipSlice;
    desc.Texture2DArray.FirstArraySlice = firstArraySlice;
    desc.Texture2DArray.ArraySize = arraySize;
    desc.Texture2DArray.PlaneSlice = planeSlice;

    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc(DXGI_FORMAT format, UINT mipLevels, UINT planeSlice)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2D.MostDetailedMip = 0;
    desc.Texture2D.MipLevels = mipLevels;
    desc.Texture2D.PlaneSlice = planeSlice;
    desc.Texture2D.ResourceMinLODClamp = 0.0f;

    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc2DArray(DXGI_FORMAT format, UINT mipLevels, UINT arraySize, UINT planeSlice)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture2DArray.MostDetailedMip = 0;
    desc.Texture2DArray.MipLevels = mipLevels;
    desc.Texture2DArray.FirstArraySlice = 0;
    desc.Texture2DArray.ArraySize = arraySize;
    desc.Texture2DArray.PlaneSlice = planeSlice;
    desc.Texture2DArray.ResourceMinLODClamp = 0.0f;

    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDesc3D(DXGI_FORMAT format, UINT mipLevels)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Texture3D.MostDetailedMip = 0;
    desc.Texture3D.MipLevels = mipLevels;
    desc.Texture3D.ResourceMinLODClamp = 0.0f;

    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetSrvDescCube(DXGI_FORMAT format, UINT mipLevels)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.TextureCube.MostDetailedMip = 0;
    desc.TextureCube.MipLevels = mipLevels;
    desc.TextureCube.ResourceMinLODClamp = 0.0f;

    return desc;
}

D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc(DXGI_FORMAT format, D3D12_DSV_FLAGS flags)
{
    D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    desc.Flags = flags;
    desc.Texture2D.MipSlice = 0;

    return desc;
}

D3D12_DEPTH_STENCIL_VIEW_DESC GetDsvDesc2DArray(DXGI_FORMAT format, UINT arraySlice, D3D12_DSV_FLAGS flags)
{
    D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};
    desc.Format = format;
    desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    desc.Flags = flags;
    desc.Texture2DArray.MipSlice = 0;
    desc.Texture2DArray.FirstArraySlice = arraySlice;
    desc.Texture2DArray.ArraySize = 1;

    return desc;
}

// Assume that intermediate resource is already mapped
void UpdateSubresources(
    ID3D12Device* pDevice,
    ID3D12GraphicsCommandList7* pCommandList,
    ID3D12Resource* pDest,
    ID3D12Resource* pIntermediate,
    UINT64 offsetInIntermediate,
    void* intermediateCpuPtr,
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
        auto pIntermediateStart = static_cast<UINT8*>(intermediateCpuPtr) + pLayouts[i].Offset;
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
                std::memcpy(pIntermediateSlice + rowPitch * y,
                       pSrcSlice + pSrcData[i].RowPitch * LONG_PTR(y),
                       pRowSizeInBytes[i]);
            }
        }
    }

    // Copy from upload heap to default heap
    // Buffer has only one subresource
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        pCommandList->CopyBufferRegion(pDest, 0, pIntermediate, offsetInIntermediate + pLayouts[0].Offset, pLayouts[0].Footprint.Width);
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
            src.pResource = pIntermediate;
            src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
            src.PlacedFootprint = pLayouts[i];
            src.PlacedFootprint.Offset += offsetInIntermediate;

            pCommandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        }
    }

    HeapFree(GetProcessHeap(), 0, pMem);
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

UINT8 GetFormatPlaneCount(ID3D12Device* pDevice, DXGI_FORMAT format)
{
    D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {format};
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
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        ret *= desc.DepthOrArraySize;
    return ret;
}

UINT GetSubresourceCount(ID3D12Device* pDevice, D3D12_RESOURCE_DESC1 desc)
{
    UINT ret = desc.MipLevels * GetFormatPlaneCount(pDevice, desc.Format);
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D)
        ret *= desc.DepthOrArraySize;
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
            DXGI_FORMAT targetDecompressedFormat = DXGI_FORMAT_UNKNOWN; // Pick default format based on the input BC format

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
} // namespace D3DHelper
