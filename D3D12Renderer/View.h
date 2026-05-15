#pragma once

#include <d3d12.h>

#include "DescriptorAllocation.h"

class ShaderResourceView
{
public:
    ShaderResourceView(ID3D12Device10* pDevice,
        ID3D12Resource* pResource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,
        DescriptorAllocation&& alloc);
    
    void Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc);
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const { return m_alloc.GetDescriptorHandle(); }

private:
    DescriptorAllocation m_alloc;
};

class RenderTargetView
{
public:
    RenderTargetView(ID3D12Device10* pDevice,
        ID3D12Resource* pResource,
        const D3D12_RENDER_TARGET_VIEW_DESC& desc,
        DescriptorAllocation&& alloc);

    void Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC& desc);
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const { return m_alloc.GetDescriptorHandle(); }

private:
    DescriptorAllocation m_alloc;
};

class DepthStencilView
{
public:
    DepthStencilView(ID3D12Device10* pDevice,
        ID3D12Resource* pResource,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& desc,
        DescriptorAllocation&& alloc);

    void Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc);
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const { return m_alloc.GetDescriptorHandle(); }

private:
    DescriptorAllocation m_alloc;
};