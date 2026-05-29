#pragma once

#include <vector>

#include <d3d12.h>
#include <minwindef.h>
#include <wrl/client.h>

class ImGuiDescriptorAllocation;

class ImGuiDescriptorAllocator
{
public:
    ImGuiDescriptorAllocator(const ImGuiDescriptorAllocator&) = delete;
    ImGuiDescriptorAllocator& operator=(const ImGuiDescriptorAllocator&) = delete;
    ImGuiDescriptorAllocator(ImGuiDescriptorAllocator&&) = delete;
    ImGuiDescriptorAllocator& operator=(ImGuiDescriptorAllocator&&) = delete;

    ImGuiDescriptorAllocator() = default;
    ~ImGuiDescriptorAllocator() = default;

    void Init(ID3D12Device* pDevice);

    ID3D12DescriptorHeap* GetDescriptorHeap();

    void Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle);
    ImGuiDescriptorAllocation Allocate();
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle);

private:
    static constexpr UINT NumDescriptors = 64;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE m_baseCpuHandle;
    D3D12_GPU_DESCRIPTOR_HANDLE m_baseGpuHandle;
    UINT m_handleIncrementSize;
    std::vector<UINT> m_freeIndices;
};
