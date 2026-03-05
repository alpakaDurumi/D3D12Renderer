#pragma once

#include <d3d12.h>

#include <vector>
#include <memory>
#include <utility>
#include <array>

#include "ConstantData.h"
#include "DescriptorAllocation.h"
#include "FrameResource.h"
#include "SharedConfig.h"

enum class TextureSlot
{
    ALBEDO,
    NORMALMAP,
    HEIGHTMAP,
    NUM_TEXTURE_SLOTS
};

class Material
{
public:
    Material(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& allocation,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources)
    {
        assert(allocation.GetNumHandles() == frameResources.size());

        auto allocations = allocation.Split();

        // Create constant buffers
        for (UINT i = 0; i < frameResources.size(); ++i)
        {
            FrameResource& frameResource = *frameResources[i];

            if (i == 0) m_constantBufferIndex = UINT(frameResource.m_materialConstantBuffers.size());
            frameResource.m_materialConstantBuffers.push_back(std::make_unique<MaterialCB>(pDevice, std::move(allocations[i])));
        }

        m_textureAddressingModes.fill(TextureAddressingMode::WRAP);
    }

    UINT GetMaterialConstantBufferIndex() const
    {
        return m_constantBufferIndex;
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

    void BuildSamplerIndices(TextureFiltering filtering)
    {
        for (UINT i = 0; i < static_cast<UINT>(TextureSlot::NUM_TEXTURE_SLOTS); ++i)
        {
            m_constantData.samplerIndices[i] = CalcSamplerIndex(filtering, m_textureAddressingModes[i]);
    }
    }

    void UpdateMaterialConstantBuffer(FrameResource& frameResource)
    {
        frameResource.m_materialConstantBuffers[m_constantBufferIndex]->Update(&m_constantData);
    }

private:
    UINT CalcSamplerIndex(TextureFiltering filtering, TextureAddressingMode addressingMode)
    {
        return static_cast<UINT>(TextureAddressingMode::NUM_TEXTURE_ADDRESSING_MODES) * static_cast<UINT>(filtering) + static_cast<UINT>(addressingMode);
    }

    MaterialConstantData m_constantData;
    UINT m_constantBufferIndex;

    std::array<TextureAddressingMode, static_cast<UINT>(TextureSlot::NUM_TEXTURE_SLOTS)> m_textureAddressingModes;
};