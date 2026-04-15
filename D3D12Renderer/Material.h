#pragma once

#include <Windows.h>

#include <d3d12.h>
#include <DirectXMath.h>

#include <array>
#include <cstddef>
#include <vector>

#include "Object.h"
#include "ConstantData.h"
#include "DescriptorAllocation.h"
#include "SharedConfig.h"

enum class TextureSlot
{
    ALBEDO,
    NORMALMAP,
    HEIGHTMAP,
    NUM_TEXTURE_SLOTS
};

enum class RenderingPath
{
    FORWARD,
    DEFERRED,
    NUM_RENDERING_PATHS
};

class Material : public Object
{
public:
    Material(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& allocation)
    {
        m_cbvAllocations = allocation.Split();

        m_textureAddressingModes.fill(TextureAddressingMode::WRAP);
    }

    void SetAmbient(XMFLOAT4 ambient)
    {
        m_constantData.SetAmbient(ambient);
    }

    void SetSpecular(XMFLOAT4 specular)
    {
        m_constantData.SetSpecular(specular);
    }

    void SetShininess(float shininess)
    {
        m_constantData.shininess = shininess;
    }

    void SetTextureIndex(TextureSlot textureSlot, UINT index)
    {
        m_constantData.textureIndices[static_cast<UINT>(textureSlot)] = index;
    }

    void SetTextureIndices(UINT albedoIdx, UINT normalMapIdx, UINT heightMapIdx)
    {
        SetTextureIndex(TextureSlot::ALBEDO, albedoIdx);
        SetTextureIndex(TextureSlot::NORMALMAP, normalMapIdx);
        SetTextureIndex(TextureSlot::HEIGHTMAP, heightMapIdx);
    }

    void SetTextureAddressingMode(TextureSlot textureSlot, TextureAddressingMode addressingMode)
    {
        m_textureAddressingModes[static_cast<UINT>(textureSlot)] = addressingMode;
    }

    void SetTextureAddressingModes(TextureAddressingMode albedoAddressingMode, TextureAddressingMode normalMapAddressingMode, TextureAddressingMode heightMapAddressingMode)
    {
        SetTextureAddressingMode(TextureSlot::ALBEDO, albedoAddressingMode);
        SetTextureAddressingMode(TextureSlot::NORMALMAP, normalMapAddressingMode);
        SetTextureAddressingMode(TextureSlot::HEIGHTMAP, heightMapAddressingMode);
    }

    void BuildSamplerIndices(TextureFiltering filtering)
    {
        for (UINT i = 0; i < static_cast<UINT>(TextureSlot::NUM_TEXTURE_SLOTS); ++i)
        {
            m_constantData.samplerIndices[i] = CalcSamplerIndex(filtering, m_textureAddressingModes[i]);
        }
    }

    void SetTextureTileScale(TextureSlot textureSlot, float tileScale)
    {
        m_constantData.textureTileScales[static_cast<UINT>(textureSlot)] = tileScale;
    }

    void SetTextureTileScales(float albedo, float normal, float height)
    {
        SetTextureTileScale(TextureSlot::ALBEDO, albedo);
        SetTextureTileScale(TextureSlot::NORMALMAP, normal);
        SetTextureTileScale(TextureSlot::HEIGHTMAP, height);
    }

    MaterialConstantData* GetConstantDataPtr()
    {
        return &m_constantData;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCBVHandle(UINT frameIndex) const
    {
        return m_cbvAllocations[frameIndex].GetDescriptorHandle();
    }

    DescriptorAllocation& GetCBVAllocationRef(UINT frameIndex)
    {
        return m_cbvAllocations[frameIndex];
    }

    void CopyDataFrom(const Material& src)
    {
        m_constantData = src.m_constantData;
        m_textureAddressingModes = src.m_textureAddressingModes;
        m_renderingPath = src.m_renderingPath;
    }

    RenderingPath GetRenderingPath() const
    {
        return m_renderingPath;
    }

    void SetRenderingPath(RenderingPath path)
    {
        m_renderingPath = path;
    }

private:
    UINT CalcSamplerIndex(TextureFiltering filtering, TextureAddressingMode addressingMode)
    {
        return static_cast<UINT>(TextureAddressingMode::NUM_TEXTURE_ADDRESSING_MODES) * static_cast<UINT>(filtering) + static_cast<UINT>(addressingMode);
    }

    MaterialConstantData m_constantData;
    std::vector<DescriptorAllocation> m_cbvAllocations;

    std::array<TextureAddressingMode, static_cast<std::size_t>(TextureSlot::NUM_TEXTURE_SLOTS)> m_textureAddressingModes;

    RenderingPath m_renderingPath;
};