#pragma once

#include <d3d12.h>

#include "DescriptorAllocation.h"

class Sampler
{
public:
    Sampler(
        ID3D12Device* pDevice,
        TextureFiltering filtering,
        TextureAddressingMode addressingMode,
        DescriptorAllocation&& allocation)
        : m_allocation(std::move(allocation))
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

        pDevice->CreateSampler(&samplerDesc, m_allocation.GetDescriptorHandle());
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle() const
    {
        return m_allocation.GetDescriptorHandle();
    }

    DescriptorAllocation& GetAllocationRef()
    {
        return m_allocation;
    }

private:
    DescriptorAllocation m_allocation;
};