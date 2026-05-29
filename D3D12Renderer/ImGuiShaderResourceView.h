#pragma once

#include <utility>

#include <d3d12.h>

#include "ImGuiDescriptorAllocation.h"

class ImGuiShaderResourceView
{
public:
    ImGuiShaderResourceView(const ImGuiShaderResourceView&) = delete;
    ImGuiShaderResourceView& operator=(const ImGuiShaderResourceView&) = delete;
    ImGuiShaderResourceView(ImGuiShaderResourceView&&) = default;
    ImGuiShaderResourceView& operator=(ImGuiShaderResourceView&&) = default;

    ImGuiShaderResourceView() = default;

    explicit ImGuiShaderResourceView(ImGuiDescriptorAllocation&& alloc)
        : m_allocation(std::move(alloc))
    {
    }

    ~ImGuiShaderResourceView() = default;

    void Init(
        ID3D12Device* pDevice,
        ID3D12Resource* pResource,
        const D3D12_SHADER_RESOURCE_VIEW_DESC& desc)
    {
        pDevice->CreateShaderResourceView(pResource, &desc, m_allocation.m_cpuHandle);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const
    {
        return m_allocation.m_cpuHandle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const
    {
        return m_allocation.m_gpuHandle;
    }

private:
    ImGuiDescriptorAllocation m_allocation;
};
