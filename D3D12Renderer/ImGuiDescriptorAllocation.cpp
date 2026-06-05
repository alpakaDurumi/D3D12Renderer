#include "pch.h"

#include "ImGuiDescriptorAllocation.h"

#include "ImGuiDescriptorAllocator.h"

ImGuiDescriptorAllocation::ImGuiDescriptorAllocation(ImGuiDescriptorAllocation&& other) noexcept
    : m_cpuHandle(other.m_cpuHandle)
    , m_gpuHandle(other.m_gpuHandle)
    , m_pAllocator(other.m_pAllocator)
{
    other.m_cpuHandle = {};
    other.m_gpuHandle = {};
    other.m_pAllocator = nullptr;
}

ImGuiDescriptorAllocation& ImGuiDescriptorAllocation::operator=(ImGuiDescriptorAllocation&& other) noexcept
{
    if (this != &other)
    {
        if (m_pAllocator != nullptr)
            m_pAllocator->Free(m_cpuHandle, m_gpuHandle);

        m_cpuHandle = other.m_cpuHandle;
        m_gpuHandle = other.m_gpuHandle;
        m_pAllocator = other.m_pAllocator;

        other.m_cpuHandle = {};
        other.m_gpuHandle = {};
        other.m_pAllocator = nullptr;
    }

    return *this;
}

ImGuiDescriptorAllocation::ImGuiDescriptorAllocation(
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle,
    ImGuiDescriptorAllocator* pAllocator)
    : m_cpuHandle(cpuHandle)
    , m_gpuHandle(gpuHandle)
    , m_pAllocator(pAllocator)
{
}

ImGuiDescriptorAllocation::~ImGuiDescriptorAllocation()
{
    if (m_pAllocator != nullptr)
        m_pAllocator->Free(m_cpuHandle, m_gpuHandle);
}
