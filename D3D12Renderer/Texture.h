#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include "DDSTextureLoader12.h"

#include <vector>

#include "D3DHelper.h"
#include "CommandList.h"
#include "UploadBuffer.h"
#include "ResourceLayoutTracker.h"
#include "DescriptorAllocator.h"
#include "DescriptorAllocation.h"
#include "Utility.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

class Texture
{
public:
    Texture(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        DescriptorAllocator& descriptorAllocator,
        UploadBuffer& uploadBuffer,
        ResourceLayoutTracker& layoutTracker,
        const std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height)
        : m_width(width), m_height(height), m_allocation(descriptorAllocator.Allocate())
    {
        CreateDefaultHeapForTexture(pDevice, width, height, m_texture);

        layoutTracker.RegisterResource(m_texture.Get(), D3D12_BARRIER_LAYOUT_COMMON, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        commandList.Barrier(
            m_texture.Get(),
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            { 0xffffffff, 0, 0, 0, 0, 0 });

        // Calculate required size for data upload
        D3D12_RESOURCE_DESC desc = m_texture->GetDesc();
        UINT64 requiredSize = 0;
        pDevice->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);

        auto uploadAllocation = uploadBuffer.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = textureSrc.data();
        textureData.RowPitch = width * 4;   // 4 bytes per pixel (RGBA)
        textureData.SlicePitch = textureData.RowPitch * height;

        UpdateSubresources(pDevice, commandList, m_texture.Get(), uploadAllocation, 0, 1, &textureData);

        commandList.Barrier(
            m_texture.Get(),
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
            { 0xffffffff, 0, 0, 0, 0, 0 });

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        pDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_allocation.GetDescriptorHandle());
    }

    Texture(
        ID3D12Device10* pDevice,
        CommandList& commandList,
        DescriptorAllocation&& allocation,
        UploadBuffer& uploadBuffer,
        ResourceLayoutTracker& layoutTracker,
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage,
        bool isCubeMap)
        : m_allocation(std::move(allocation))
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
            ConvertToDDS(filePath, isSRGB, useBlockCompress, flipImage);
        }

        std::unique_ptr<uint8_t[]> ddsData;
        std::vector<D3D12_SUBRESOURCE_DATA> subresources;

        ThrowIfFailed(DirectX::LoadDDSTextureFromFile(
            pDevice,
            ddsFilePath.c_str(),
            &m_texture,
            ddsData,
            subresources));

        D3D12_RESOURCE_DESC desc = m_texture->GetDesc();
        UINT numSubresources = static_cast<UINT>(subresources.size());

        layoutTracker.RegisterResource(m_texture.Get(), D3D12_BARRIER_LAYOUT_COMMON, desc.DepthOrArraySize, desc.MipLevels, desc.Format);

        commandList.Barrier(
            m_texture.Get(),
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            { 0xffffffff, 0, 0, 0, 0, 0 });

        // Calculate required size for data upload
        UINT64 requiredSize = 0;
        pDevice->GetCopyableFootprints(&desc, 0, numSubresources, 0, nullptr, nullptr, nullptr, &requiredSize);

        auto uploadAllocation = uploadBuffer.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        UpdateSubresources(pDevice, commandList, m_texture.Get(), uploadAllocation, 0, numSubresources, subresources.data());

        commandList.Barrier(
            m_texture.Get(),
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
            { 0xffffffff, 0, 0, 0, 0, 0 });

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

        pDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_allocation.GetDescriptorHandle());

        m_width = static_cast<UINT>(desc.Width);
        m_height = static_cast<UINT>(desc.Height);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle()
    {
        return m_allocation.GetDescriptorHandle();
    }

private:
    ComPtr<ID3D12Resource> m_texture;
    DescriptorAllocation m_allocation;

    UINT m_width;
    UINT m_height;
};