#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include <vector>
#include <string>

#include "DescriptorAllocation.h"
#include "TransientUploadAllocator.h"

using Microsoft::WRL::ComPtr;

class Texture
{
public:
    Texture(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& allocation,
        TransientUploadAllocator& uploadAllocator,
        const std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height);

    Texture(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& allocation,
        TransientUploadAllocator& uploadAllocator,
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage,
        bool isCubeMap);

    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle();
    DescriptorAllocation& GetAllocationRef();

private:
    ComPtr<ID3D12Resource> m_texture;
    DescriptorAllocation m_allocation;

    UINT m_width;
    UINT m_height;
};