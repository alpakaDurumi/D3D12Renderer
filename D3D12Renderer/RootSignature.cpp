#include "pch.h"
#include "RootSignature.h"

#include "D3DHelper.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

RootParameter::RootParameter(RootParameter&& other) noexcept = default;
RootParameter& RootParameter::operator=(RootParameter&& other) noexcept = default;

// Explicitly initialize with 0
// Since m_paramter is POD, it could be garbage value and desctructor may occurs undefined behaviour
RootParameter::RootParameter()
    : m_parameter{}
{
}

RootParameter::~RootParameter() = default;

void RootParameter::InitAsConstant(UINT reg, UINT space, UINT num, D3D12_SHADER_VISIBILITY visibility)
{
    m_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    m_parameter.Constants.ShaderRegister = reg;
    m_parameter.Constants.RegisterSpace = space;
    m_parameter.Constants.Num32BitValues = num;
    m_parameter.ShaderVisibility = visibility;
}

void RootParameter::InitAsDescriptor(UINT reg, UINT space, D3D12_SHADER_VISIBILITY visibility, D3D12_ROOT_PARAMETER_TYPE type, D3D12_ROOT_DESCRIPTOR_FLAGS flags)
{
    m_parameter.ParameterType = type;
    m_parameter.Descriptor.ShaderRegister = reg;
    m_parameter.Descriptor.RegisterSpace = space;
    m_parameter.Descriptor.Flags = flags;
    m_parameter.ShaderVisibility = visibility;
}

void RootParameter::InitAsTable(UINT numRanges, D3D12_SHADER_VISIBILITY visibility)
{
    m_ranges.resize(numRanges);
    m_parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    m_parameter.DescriptorTable.NumDescriptorRanges = numRanges;
    m_parameter.DescriptorTable.pDescriptorRanges = m_ranges.data();
    m_parameter.ShaderVisibility = visibility;
}

void RootParameter::InitAsRange(UINT rangeIndex, UINT reg, UINT space, D3D12_DESCRIPTOR_RANGE_TYPE type, UINT numDescriptors, D3D12_DESCRIPTOR_RANGE_FLAGS flags)
{
    auto& range = m_ranges[rangeIndex];
    range.RangeType = type;
    range.NumDescriptors = numDescriptors;
    range.BaseShaderRegister = reg;
    range.RegisterSpace = space;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    range.Flags = flags;
}

RootSignature::RootSignature(UINT numParameters, UINT numStaticSamplers)
    : m_numParameters(numParameters), m_numStaticSamplers(numStaticSamplers)
{
    assert(numParameters <= 16);    // Maximum number of parameters is limited to 16

    if (m_numParameters > 0)
        m_parameters.reset(new RootParameter[m_numParameters]);
    else
        m_parameters = nullptr;

    if (m_numStaticSamplers > 0)
        m_staticSamplers.reset(new D3D12_STATIC_SAMPLER_DESC[m_numStaticSamplers]);
    else
        m_staticSamplers = nullptr;
}

ID3D12RootSignature* RootSignature::GetRootSignature() const
{
    return m_rootSignature.Get();
}

UINT RootSignature::GetNumParameters() const
{
    return m_numParameters;
}

UINT RootSignature::GetNumStaticSamplers() const
{
    return m_numStaticSamplers;
}

UINT32 RootSignature::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE heapType) const
{
    return heapType == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? m_cbvSrvUavTableBitMask : m_samplerTableBitMask;
}
UINT32 RootSignature::GetTableSize(UINT parameterIndex) const
{
    assert(parameterIndex < m_numParameters);
    return m_descriptorTableSize[parameterIndex];
}

RootParameter& RootSignature::operator[](SIZE_T parameterIndex)
{
    assert(parameterIndex < m_numParameters);
    return m_parameters.get()[parameterIndex];
}

// filtering argument ignored if comparisonFunc set.
void RootSignature::InitStaticSampler(
    UINT reg,
    UINT space,
    UINT samplerIndex,
    D3D12_SHADER_VISIBILITY visibility,
    TextureFiltering filtering,
    TextureAddressingMode addressingMode,
    D3D12_COMPARISON_FUNC comparisonFunc)
{
    D3D12_STATIC_SAMPLER_DESC& samplerDesc = m_staticSamplers[samplerIndex];

    bool doCompare = comparisonFunc != D3D12_COMPARISON_FUNC_NONE;

    // If comparisonFunc set, always perform hardware PCF.
    if (doCompare)
    {
        samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        samplerDesc.MaxAnisotropy = 0;
    }
    else
    {
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
    samplerDesc.ComparisonFunc = comparisonFunc;
    samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    samplerDesc.ShaderRegister = reg;
    samplerDesc.RegisterSpace = space;
    samplerDesc.ShaderVisibility = visibility;
}

void RootSignature::Finalize(ID3D12Device10* pDevice)
{
    // Fill bitmasks
    for (UINT i = 0; i < m_numParameters; ++i)
    {
        D3D12_ROOT_PARAMETER1& param = m_parameters[i].m_parameter;
        m_descriptorTableSize[i] = 0;

        if (param.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            // 한 table 내에 속한 모든 range의 type이 같다고 가정
            if (param.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
            {
                m_samplerTableBitMask |= (1 << i);
            }
            else
            {
                m_cbvSrvUavTableBitMask |= (1 << i);
            }

            for (UINT rangeIndex = 0; rangeIndex < param.DescriptorTable.NumDescriptorRanges; ++rangeIndex)
                m_descriptorTableSize[i] += param.DescriptorTable.pDescriptorRanges[rangeIndex].NumDescriptors;
        }
    }

    // Use D3D_ROOT_SIGNATURE_VERSION_1_1 if current environment supports it
    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
    {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    D3D12_ROOT_SIGNATURE_FLAGS flag =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    // Downgraded objects must not be destroyed until CreateRootSignature
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    std::vector<D3D12_ROOT_PARAMETER> downgradedParameters;
    std::vector<D3D12_DESCRIPTOR_RANGE> convertedRanges;

    std::vector<D3D12_ROOT_PARAMETER1> parameters(m_numParameters);
    for (UINT i = 0; i < m_numParameters; ++i)
        parameters[i] = m_parameters[i].m_parameter;

    if (featureData.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_1)
    {
        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        rootSignatureDesc.Desc_1_1.NumParameters = m_numParameters;
        rootSignatureDesc.Desc_1_1.pParameters = m_numParameters == 0 ? nullptr : parameters.data();
        rootSignatureDesc.Desc_1_1.NumStaticSamplers = m_numStaticSamplers;
        rootSignatureDesc.Desc_1_1.pStaticSamplers = m_staticSamplers.get();
        rootSignatureDesc.Desc_1_1.Flags = flag;
    }
    else
    {
        downgradedParameters.resize(m_numParameters);

        UINT offset = 0;
        DowngradeRootParameters(parameters.data(), m_numParameters, downgradedParameters.data(), convertedRanges, offset);

        rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
        rootSignatureDesc.Desc_1_0.NumParameters = m_numParameters;
        rootSignatureDesc.Desc_1_0.pParameters = m_numParameters == 0 ? nullptr : downgradedParameters.data();
        rootSignatureDesc.Desc_1_0.NumStaticSamplers = m_numStaticSamplers;
        rootSignatureDesc.Desc_1_0.pStaticSamplers = m_staticSamplers.get();
        rootSignatureDesc.Desc_1_0.Flags = flag;
    }

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
    ThrowIfFailed(pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}