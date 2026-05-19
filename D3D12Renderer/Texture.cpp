#include "pch.h"
#include "Texture.h"

#include "D3DHelper.h"

using namespace D3DHelper;

Texture::Texture(
    ID3D12Device10* pDevice,
    const D3D12_RESOURCE_DESC1& desc,
    D3D12_BARRIER_LAYOUT initialLayout,
    const D3D12_CLEAR_VALUE* pClearValue,
    D3D12_HEAP_TYPE heapType)
{
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = heapType;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    ThrowIfFailed(pDevice->CreateCommittedResource3(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        initialLayout,
        pClearValue,
        nullptr,
        0,
        nullptr,
        IID_PPV_ARGS(&m_resource)));
}