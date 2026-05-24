#pragma once

#include <minwindef.h>

#include <d3d12.h>

#include "DescriptorAllocation.h"

class View
{
public:
    View() = default;

    explicit View(DescriptorAllocation&& alloc);

    View(const View&) = delete;
    View& operator=(const View&) = delete;
    View(View&&) = default;
    View& operator=(View&&) = default;

    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle() const;

protected:
    DescriptorAllocation m_alloc;
};

class ShaderResourceView : public View
{
public:
    using View::View;

    ShaderResourceView(
        ID3D12Device10* pDevice,
        ID3D12Resource* pResource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& desc,
        DescriptorAllocation&& alloc);

    void Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC& desc);
};

class RenderTargetView : public View
{
public:
    using View::View;

    RenderTargetView(
        ID3D12Device10* pDevice,
        ID3D12Resource* pResource,
        const D3D12_RENDER_TARGET_VIEW_DESC& desc,
        DescriptorAllocation&& alloc);

    void Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC& desc);
};

class DepthStencilView : public View
{
public:
    using View::View;

    DepthStencilView(
        ID3D12Device10* pDevice,
        ID3D12Resource* pResource,
        const D3D12_DEPTH_STENCIL_VIEW_DESC& desc,
        DescriptorAllocation&& alloc);

    void Init(ID3D12Device10* pDevice, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC& desc);
};

class ConstantBufferView : public View
{
public:
    using View::View;

    ConstantBufferView(
        ID3D12Device10* pDevice,
        D3D12_GPU_VIRTUAL_ADDRESS gpuPtr,
        UINT size,
        DescriptorAllocation&& alloc);

    void Init(ID3D12Device10* pDevice, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr, UINT size);
};
