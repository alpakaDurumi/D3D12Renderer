#include "pch.h"
#include "AssetTexture.h"

#include "D3DHelper.h"
#include "DDSTextureLoader12.h"
#include "Utility.h"

AssetTexture::AssetTexture(
    ID3D12Device10* pDevice,
    ID3D12GraphicsCommandList7* pCommandList,
    DescriptorAllocation&& srvAllocation,
    TransientUploadAllocator& uploadAllocator,
    const std::vector<UINT8>& textureSrc,
    UINT width,
    UINT height)
    : Texture(std::move(srvAllocation))
{
    D3DHelper::CreateDefaultTexture(pDevice, width, height, m_resource);

    D3D12_TEXTURE_BARRIER barrier0 = {
        D3D12_BARRIER_SYNC_NONE,
        D3D12_BARRIER_SYNC_COPY,
        D3D12_BARRIER_ACCESS_NO_ACCESS,
        D3D12_BARRIER_ACCESS_COPY_DEST,
        D3D12_BARRIER_LAYOUT_COMMON,
        D3D12_BARRIER_LAYOUT_COPY_DEST,
        Get(),
        {0xffff'ffff, 0, 0, 0, 0, 0},
        D3D12_TEXTURE_BARRIER_FLAG_NONE
    };

    D3D12_BARRIER_GROUP barrierGroups0[] = { D3DHelper::TextureBarrierGroup(1, &barrier0) };
    pCommandList->Barrier(1, barrierGroups0);

    // Calculate required size for data upload
    D3D12_RESOURCE_DESC desc = m_resource->GetDesc();
    UINT64 requiredSize = 0;
    pDevice->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);

    auto uploadAllocation = uploadAllocator.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = textureSrc.data();
    textureData.RowPitch = width * 4;   // 4 bytes per pixel (RGBA)
    textureData.SlicePitch = textureData.RowPitch * height;

    D3DHelper::UpdateSubresources(pDevice, pCommandList, Get(), uploadAllocation, 0, 1, &textureData);

    D3D12_TEXTURE_BARRIER barrier1 = {
        D3D12_BARRIER_SYNC_COPY,
        D3D12_BARRIER_SYNC_PIXEL_SHADING,
        D3D12_BARRIER_ACCESS_COPY_DEST,
        D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
        D3D12_BARRIER_LAYOUT_COPY_DEST,
        D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
        Get(),
        {0xffff'ffff, 0, 0, 0, 0, 0},
        D3D12_TEXTURE_BARRIER_FLAG_NONE
    };

    D3D12_BARRIER_GROUP barrierGroups1[] = { D3DHelper::TextureBarrierGroup(1, &barrier1) };
    pCommandList->Barrier(1, barrierGroups1);

    // Describe and create a SRV for the texture.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;
    
    pDevice->CreateShaderResourceView(m_resource.Get(), &srvDesc, GetSRVHandle());
}

AssetTexture::AssetTexture(
    ID3D12Device10* pDevice,
    ID3D12GraphicsCommandList7* pCommandList,
    DescriptorAllocation&& srvAllocation,
    TransientUploadAllocator& uploadAllocator,
    const std::wstring& filePath,
    bool isSRGB,
    bool useBlockCompress,
    bool flipImage,
    bool isCubeMap)
    : Texture(std::move(srvAllocation))
{
    // Find file and check validity
    std::wstring ddsFilePath = Utility::RemoveFileExtension(filePath) + L".dds";

    struct _stat64 ddsFileStat, srcFileStat;

    bool srcFileMissing = _wstat64(filePath.c_str(), &srcFileStat) == -1;
    bool ddsFileMissing = _wstat64(ddsFilePath.c_str(), &ddsFileStat) == -1;

    if (srcFileMissing)
    {
        throw std::runtime_error("File not found.");
    }

    // If dds file does not exist or older than src file
    if (ddsFileMissing || ddsFileStat.st_mtime < srcFileStat.st_mtime)
    {
        D3DHelper::ConvertToDDS(filePath, isSRGB, useBlockCompress, flipImage);
    }

    std::unique_ptr<uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    D3DHelper::ThrowIfFailed(DirectX::LoadDDSTextureFromFile(
        pDevice,
        ddsFilePath.c_str(),
        &m_resource,
        ddsData,
        subresources));

    D3D12_RESOURCE_DESC desc = m_resource->GetDesc();
    UINT numSubresources = static_cast<UINT>(subresources.size());

    D3D12_TEXTURE_BARRIER barrier0 = {
        D3D12_BARRIER_SYNC_NONE,
        D3D12_BARRIER_SYNC_COPY,
        D3D12_BARRIER_ACCESS_NO_ACCESS,
        D3D12_BARRIER_ACCESS_COPY_DEST,
        D3D12_BARRIER_LAYOUT_COMMON,
        D3D12_BARRIER_LAYOUT_COPY_DEST,
        Get(),
        {0xffff'ffff, 0, 0, 0, 0, 0},
        D3D12_TEXTURE_BARRIER_FLAG_NONE
    };

    D3D12_BARRIER_GROUP barrierGroups0[] = { D3DHelper::TextureBarrierGroup(1, &barrier0) };
    pCommandList->Barrier(1, barrierGroups0);

    // Calculate required size for data upload
    UINT64 requiredSize = 0;
    pDevice->GetCopyableFootprints(&desc, 0, numSubresources, 0, nullptr, nullptr, nullptr, &requiredSize);

    auto uploadAllocation = uploadAllocator.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    D3DHelper::UpdateSubresources(pDevice, pCommandList, Get(), uploadAllocation, 0, numSubresources, subresources.data());

    D3D12_TEXTURE_BARRIER barrier1 = {
        D3D12_BARRIER_SYNC_COPY,
        D3D12_BARRIER_SYNC_PIXEL_SHADING,
        D3D12_BARRIER_ACCESS_COPY_DEST,
        D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
        D3D12_BARRIER_LAYOUT_COPY_DEST,
        D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
        Get(),
        {0xffff'ffff, 0, 0, 0, 0, 0},
        D3D12_TEXTURE_BARRIER_FLAG_NONE
    };

    D3D12_BARRIER_GROUP barrierGroups1[] = { D3DHelper::TextureBarrierGroup(1, &barrier1) };
    pCommandList->Barrier(1, barrierGroups1);

    // Describe and create a SRV for the texture.
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = desc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
    {
        if (desc.DepthOrArraySize == 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
            srvDesc.Texture1D.MipLevels = desc.MipLevels;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
            srvDesc.Texture1DArray.MipLevels = desc.MipLevels;
        }
    }
    else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
    {
        if (desc.DepthOrArraySize == 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = desc.MipLevels;
        }
        else if (desc.DepthOrArraySize % 6 == 0 && isCubeMap)
        {
            if (desc.DepthOrArraySize / 6 == 1)
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                srvDesc.TextureCube.MipLevels = desc.MipLevels;
            }
            else
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                srvDesc.TextureCubeArray.MipLevels = desc.MipLevels;
            }
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
        }
    }
    else    // TEXTURE3D
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
        srvDesc.Texture3D.MipLevels = desc.MipLevels;
    }

    pDevice->CreateShaderResourceView(Get(), &srvDesc, GetSRVHandle());
}