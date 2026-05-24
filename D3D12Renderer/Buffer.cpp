#include "pch.h"

#include "Buffer.h"

#include "D3DHelper.h"

using namespace D3DHelper;

Buffer::Buffer(ID3D12Device10* pDevice, UINT64 width, D3D12_HEAP_TYPE heapType)
{
    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = heapType;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC1 resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = width;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc = {1, 0};
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    resourceDesc.SamplerFeedbackMipRegion = {}; // Not use Sampler Feedback

    ThrowIfFailed(pDevice->CreateCommittedResource3(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_BARRIER_LAYOUT_UNDEFINED,
        nullptr,
        nullptr,
        0,
        nullptr,
        IID_PPV_ARGS(&m_resource)));
}
