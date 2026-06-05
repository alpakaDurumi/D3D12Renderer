#pragma once

#include <d3d12.h>

class ImGuiDescriptorAllocator;

class ImGuiDescriptorAllocation
{
    friend class ImGuiShaderResourceView;

public:
    ImGuiDescriptorAllocation(const ImGuiDescriptorAllocation&) = delete;
    ImGuiDescriptorAllocation& operator=(const ImGuiDescriptorAllocation&) = delete;
    ImGuiDescriptorAllocation(ImGuiDescriptorAllocation&& other) noexcept;
    ImGuiDescriptorAllocation& operator=(ImGuiDescriptorAllocation&& other) noexcept;

    ImGuiDescriptorAllocation() = default;

    ImGuiDescriptorAllocation(
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
        ImGuiDescriptorAllocator* pAllocator);

    ~ImGuiDescriptorAllocation();

private:
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHandle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHandle = {};

    ImGuiDescriptorAllocator* m_pAllocator = nullptr;
};
