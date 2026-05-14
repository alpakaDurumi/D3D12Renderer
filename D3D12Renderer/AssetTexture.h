#pragma once

#include <string>

#include "Texture.h"
#include "TransientUploadAllocator.h"

class AssetTexture : public Texture
{
public:
    AssetTexture(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& srvAllocation,
        TransientUploadAllocator& uploadAllocator,
        const std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height);

    AssetTexture(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& srvAllocation,
        TransientUploadAllocator& uploadAllocator,
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage,
        bool isCubeMap);
};