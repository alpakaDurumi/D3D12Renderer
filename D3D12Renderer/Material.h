#pragma once

#include <array>
#include <cstddef>

#include <DirectXMath.h>
#include <d3d12.h>
#include <minwindef.h>

#include "ConstantData.h"
#include "RendererConfig.h"
#include "View.h"

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

class DescriptorAllocation;

class Material
{
public:
    Material(DescriptorAllocation&& allocation);

    void SetAmbient(DirectX::XMFLOAT4 ambient);
    void SetSpecular(DirectX::XMFLOAT4 specular);
    void SetShininess(float shininess);

    void SetTextureIndex(TextureSlot textureSlot, UINT index);
    void SetTextureIndices(UINT albedoIdx, UINT normalMapIdx, UINT heightMapIdx);

    void SetTextureAddressingMode(TextureSlot textureSlot, TextureAddressingMode addressingMode);
    void SetTextureAddressingModes(TextureAddressingMode albedoAddressingMode, TextureAddressingMode normalMapAddressingMode, TextureAddressingMode heightMapAddressingMode);

    void BuildSamplerIndices(TextureFiltering filtering);

    void SetTextureTileScale(TextureSlot textureSlot, float tileScale);
    void SetTextureTileScales(float albedo, float normal, float height);

    MaterialConstantData* GetConstantDataPtr();

    D3D12_CPU_DESCRIPTOR_HANDLE GetCbvHandle() const;

    void InitCbv(ID3D12Device10* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr);

    void CopyDataFrom(const Material& src);

    RenderingPath GetRenderingPath() const;

    void SetRenderingPath(RenderingPath path);

private:
    UINT CalcSamplerIndex(TextureFiltering filtering, TextureAddressingMode addressingMode);

    MaterialConstantData m_constantData;
    ConstantBufferView m_cbv;

    std::array<TextureAddressingMode, static_cast<std::size_t>(TextureSlot::NUM_TEXTURE_SLOTS)> m_textureAddressingModes;

    RenderingPath m_renderingPath;
};
