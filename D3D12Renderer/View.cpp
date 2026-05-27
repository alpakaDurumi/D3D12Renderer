#include "pch.h"

#include "View.h"

View::View(DescriptorAllocation&& alloc)
    : m_alloc(std::move(alloc))
{
    assert(m_alloc.GetNumHandles() == 1);
}

D3D12_CPU_DESCRIPTOR_HANDLE View::GetHandle() const
{
    return m_alloc.GetDescriptorHandle();
}

ShaderResourceView::ShaderResourceView(
    ID3D12Device* pDevice,
    ID3D12Resource* pResource,
    const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,
    DescriptorAllocation&& alloc)
    : View(std::move(alloc))
{
    Init(pDevice, pResource, desc);
}

void ShaderResourceView::Init(ID3D12Device* pDevice, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
    pDevice->CreateShaderResourceView(pResource, &desc, m_alloc.GetDescriptorHandle());
}

RenderTargetView::RenderTargetView(
    ID3D12Device* pDevice,
    ID3D12Resource* pResource,
    const D3D12_RENDER_TARGET_VIEW_DESC& desc,
    DescriptorAllocation&& alloc)
    : View(std::move(alloc))
{
    Init(pDevice, pResource, desc);
}

void RenderTargetView::Init(ID3D12Device* pDevice, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC& desc)
{
    pDevice->CreateRenderTargetView(pResource, &desc, m_alloc.GetDescriptorHandle());
}

DepthStencilView::DepthStencilView(
    ID3D12Device* pDevice,
    ID3D12Resource* pResource,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& desc,
    DescriptorAllocation&& alloc)
    : View(std::move(alloc))
{
    Init(pDevice, pResource, desc);
}

void DepthStencilView::Init(ID3D12Device* pDevice, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc)
{
    pDevice->CreateDepthStencilView(pResource, &desc, m_alloc.GetDescriptorHandle());
}

ConstantBufferView::ConstantBufferView(
    ID3D12Device* pDevice,
    D3D12_GPU_VIRTUAL_ADDRESS gpuPtr,
    UINT size,
    DescriptorAllocation&& alloc)
    : View(std::move(alloc))
{
    Init(pDevice, gpuPtr, size);
}

void ConstantBufferView::Init(ID3D12Device* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr, UINT size)
{
    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = gpuPtr;
    cbvDesc.SizeInBytes = size;

    pDevice->CreateConstantBufferView(&cbvDesc, m_alloc.GetDescriptorHandle());
}
