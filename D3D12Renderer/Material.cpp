#include "pch.h"

#include "Material.h"

Material::Material(DescriptorAllocation&& allocation)
    : m_cbv(std::move(allocation))
{
    m_textureAddressingModes.fill(TextureAddressingMode::WRAP);
}

void Material::SetAmbient(DirectX::XMFLOAT4 ambient)
{
    m_constantData.SetAmbient(ambient);
}

void Material::SetSpecular(DirectX::XMFLOAT4 specular)
{
    m_constantData.SetSpecular(specular);
}

void Material::SetShininess(float shininess)
{
    m_constantData.shininess = shininess;
}

void Material::SetTextureIndex(TextureSlot textureSlot, UINT index)
{
    m_constantData.textureIndices[static_cast<UINT>(textureSlot)] = index;
}

void Material::SetTextureIndices(UINT albedoIdx, UINT normalMapIdx, UINT heightMapIdx)
{
    SetTextureIndex(TextureSlot::ALBEDO, albedoIdx);
    SetTextureIndex(TextureSlot::NORMALMAP, normalMapIdx);
    SetTextureIndex(TextureSlot::HEIGHTMAP, heightMapIdx);
}

void Material::SetTextureAddressingMode(TextureSlot textureSlot, TextureAddressingMode addressingMode)
{
    m_textureAddressingModes[static_cast<UINT>(textureSlot)] = addressingMode;
}

void Material::SetTextureAddressingModes(TextureAddressingMode albedoAddressingMode, TextureAddressingMode normalMapAddressingMode, TextureAddressingMode heightMapAddressingMode)
{
    SetTextureAddressingMode(TextureSlot::ALBEDO, albedoAddressingMode);
    SetTextureAddressingMode(TextureSlot::NORMALMAP, normalMapAddressingMode);
    SetTextureAddressingMode(TextureSlot::HEIGHTMAP, heightMapAddressingMode);
}

void Material::BuildSamplerIndices(TextureFiltering filtering)
{
    for (UINT i = 0; i < static_cast<UINT>(TextureSlot::NUM_TEXTURE_SLOTS); ++i)
    {
        m_constantData.samplerIndices[i] = CalcSamplerIndex(filtering, m_textureAddressingModes[i]);
    }
}

void Material::SetTextureTileScale(TextureSlot textureSlot, float tileScale)
{
    m_constantData.textureTileScales[static_cast<UINT>(textureSlot)] = tileScale;
}

void Material::SetTextureTileScales(float albedo, float normal, float height)
{
    SetTextureTileScale(TextureSlot::ALBEDO, albedo);
    SetTextureTileScale(TextureSlot::NORMALMAP, normal);
    SetTextureTileScale(TextureSlot::HEIGHTMAP, height);
}

MaterialConstantData* Material::GetConstantDataPtr()
{
    return &m_constantData;
}

D3D12_CPU_DESCRIPTOR_HANDLE Material::GetCbvHandle() const
{
    return m_cbv.GetHandle();
}

void Material::InitCbv(ID3D12Device* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr)
{
    m_cbv.Init(pDevice, gpuPtr, sizeof(MaterialConstantData));
}

void Material::CopyDataFrom(const Material& src)
{
    m_constantData = src.m_constantData;
    m_textureAddressingModes = src.m_textureAddressingModes;
    m_renderingPath = src.m_renderingPath;
}

RenderingPath Material::GetRenderingPath() const
{
    return m_renderingPath;
}

void Material::SetRenderingPath(RenderingPath path)
{
    m_renderingPath = path;
}

UINT Material::CalcSamplerIndex(TextureFiltering filtering, TextureAddressingMode addressingMode)
{
    return static_cast<UINT>(TextureAddressingMode::NUM_TEXTURE_ADDRESSING_MODES) * static_cast<UINT>(filtering) + static_cast<UINT>(addressingMode);
}
