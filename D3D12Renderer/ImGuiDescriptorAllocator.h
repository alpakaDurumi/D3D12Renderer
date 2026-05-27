#pragma once

#include <cassert>
#include <vector>

#include <d3d12.h>
#include <minwindef.h>
#include <wrl/client.h>

#include "D3DHelper.h"

class ImGuiDescriptorAllocator
{
public:
    ImGuiDescriptorAllocator(const ImGuiDescriptorAllocator&) = delete;
    ImGuiDescriptorAllocator& operator=(const ImGuiDescriptorAllocator&) = delete;
    ImGuiDescriptorAllocator(ImGuiDescriptorAllocator&&) = delete;
    ImGuiDescriptorAllocator& operator=(ImGuiDescriptorAllocator&&) = delete;

    ImGuiDescriptorAllocator() = default;
    ~ImGuiDescriptorAllocator() = default;

    void Init(ID3D12Device* pDevice)
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = NumDescriptors;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        D3DHelper::ThrowIfFailed(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));

        heapStartCpu = m_heap->GetCPUDescriptorHandleForHeapStart();
        heapStartGpu = m_heap->GetGPUDescriptorHandleForHeapStart();
        heapHandleIncrement = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        freeIndices.resize(NumDescriptors);
        for (UINT i = 0; i < NumDescriptors; ++i)
            freeIndices[i] = i;
    }

    void Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        assert(!freeIndices.empty());
        int idx = freeIndices.back();
        freeIndices.pop_back();
        *out_cpu_desc_handle = D3DHelper::GetCpuDescriptorHandle(heapStartCpu, idx, heapHandleIncrement);
        *out_gpu_desc_handle = D3DHelper::GetGpuDescriptorHandle(heapStartGpu, idx, heapHandleIncrement);
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
    {
        int cpu_idx = (int)((out_cpu_desc_handle.ptr - heapStartCpu.ptr) / heapHandleIncrement);
        int gpu_idx = (int)((out_gpu_desc_handle.ptr - heapStartGpu.ptr) / heapHandleIncrement);
        assert(cpu_idx == gpu_idx);
        freeIndices.push_back(cpu_idx);
    }

    ID3D12DescriptorHeap* GetDescriptorHeap()
    {
        return m_heap.Get();
    }

private:
    static constexpr UINT NumDescriptors = 64;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE heapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE heapStartGpu;
    UINT heapHandleIncrement;
    std::vector<UINT> freeIndices;
};
