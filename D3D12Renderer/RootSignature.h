#pragma once

#include <minwindef.h>
#include <basetsd.h>
#include <wrl/client.h>

#include <d3d12.h>

#include <vector>

#include "RendererConfig.h"

class RootParameter
{
    friend class RootSignature;

public:
    RootParameter(const RootParameter&) = delete;
    RootParameter& operator=(const RootParameter&) = delete;
    RootParameter(RootParameter&& other) noexcept;
    RootParameter& operator=(RootParameter&& other) noexcept;

    RootParameter();
    ~RootParameter();

    void InitAsConstant(UINT reg, UINT space, UINT num, D3D12_SHADER_VISIBILITY visibility);
    void InitAsDescriptor(UINT reg, UINT space, D3D12_SHADER_VISIBILITY visibility, D3D12_ROOT_PARAMETER_TYPE type, D3D12_ROOT_DESCRIPTOR_FLAGS flags);
    void InitAsTable(UINT numRanges, D3D12_SHADER_VISIBILITY visibility);
    void InitAsRange(UINT rangeIndex, UINT reg, UINT space, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_RANGE_FLAGS flags);

private:
    D3D12_ROOT_PARAMETER1 m_parameter;
    std::vector<D3D12_DESCRIPTOR_RANGE1> m_ranges;
};

class RootSignature
{
public:
    RootSignature(UINT numParameters, UINT numStaticSamplers);

    ID3D12RootSignature* GetRootSignature() const;
    UINT GetNumParameters() const;
    UINT GetNumStaticSamplers() const;
    UINT32 GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE heapType) const;
    UINT32 GetTableSize(UINT parameterIndex) const;

    RootParameter& operator[](SIZE_T parameterIndex);

    void InitStaticSampler(
        UINT reg,
        UINT space,
        UINT samplerIndex,
        D3D12_SHADER_VISIBILITY visibility,
        TextureFiltering filtering,
        TextureAddressingMode addressingMode,
        D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_NONE);

    void Finalize(ID3D12Device10* pDevice);

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    UINT m_numParameters;
    UINT m_numStaticSamplers;

    UINT32 m_cbvSrvUavTableBitMask = 0;
    UINT32 m_samplerTableBitMask = 0;
    UINT32 m_descriptorTableSize[16] = {};  // Maximum number of tables is limited to 16

    std::vector<RootParameter> m_parameters;
    std::vector<D3D12_STATIC_SAMPLER_DESC> m_staticSamplers;
};