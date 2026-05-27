#pragma once

#include <vector>

#include <basetsd.h>
#include <d3d12.h>
#include <minwindef.h>
#include <wrl/client.h>

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
    RootSignature(const RootSignature&) = delete;
    RootSignature& operator=(const RootSignature&) = delete;
    RootSignature(RootSignature&&) = delete;
    RootSignature& operator=(RootSignature&&) = delete;

    RootSignature() = default;
    ~RootSignature() = default;

    void Init(UINT numParameters, UINT numStaticSamplers);

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
    static constexpr UINT MaxNumParameters = 16;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;

    UINT m_numParameters = 0;
    UINT m_numStaticSamplers = 0;

    UINT32 m_cbvSrvUavTableBitMask = 0;
    UINT32 m_samplerTableBitMask = 0;

    std::vector<RootParameter> m_parameters;
    std::vector<UINT32> m_descriptorTableSize;

    std::vector<D3D12_STATIC_SAMPLER_DESC> m_staticSamplers;
};
