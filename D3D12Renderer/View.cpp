#include "pch.h"
#include "View.h"

ShaderResourceView::ShaderResourceView(
    ID3D12Device10* pDevice,
    ID3D12Resource* pResource,
    const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,
    DescriptorAllocation&& alloc)
    : m_alloc(std::move(alloc))
{
    assert(!m_alloc.IsNull());
    Init(pDevice, pResource, desc);
}

void ShaderResourceView::Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
{
    pDevice->CreateShaderResourceView(pResource, &desc, m_alloc.GetDescriptorHandle());
}

RenderTargetView::RenderTargetView(
    ID3D12Device10* pDevice,
    ID3D12Resource* pResource,
    const D3D12_RENDER_TARGET_VIEW_DESC& desc,
    DescriptorAllocation&& alloc)
    : m_alloc(std::move(alloc))
{
    assert(!m_alloc.IsNull());
    Init(pDevice, pResource, desc);
}

void RenderTargetView::Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC& desc)
{
    pDevice->CreateRenderTargetView(pResource, &desc, m_alloc.GetDescriptorHandle());
}

DepthStencilView::DepthStencilView(
    ID3D12Device10* pDevice,
    ID3D12Resource* pResource,
    const D3D12_DEPTH_STENCIL_VIEW_DESC& desc,
    DescriptorAllocation&& alloc)
    : m_alloc(std::move(alloc))
{
    assert(!m_alloc.IsNull());
    Init(pDevice, pResource, desc);
}

void DepthStencilView::Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc)
{
    pDevice->CreateDepthStencilView(pResource, &desc, m_alloc.GetDescriptorHandle());
}