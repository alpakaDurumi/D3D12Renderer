#pragma once

#include "wrl/client.h"

#include "d3d12.h"

#include <cassert>
#include <vector>

#include "D3DHelper.h"

using namespace D3DHelper;

class ImGuiDescriptorAllocator
{
public:
    ImGuiDescriptorAllocator(ID3D12Device* pDevice)
    {
        // Create descriptor heap for ImGui
        const UINT ImGuiHeapSize = 64;
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = ImGuiHeapSize;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));

        HeapStartCpu = m_heap->GetCPUDescriptorHandleForHeapStart();
        HeapStartGpu = m_heap->GetGPUDescriptorHandleForHeapStart();
        HeapHandleIncrement = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        FreeIndices.reserve(ImGuiHeapSize);
        for (UINT i = 0; i < ImGuiHeapSize; i++)
            FreeIndices.push_back(i);
    }

    void Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle)
    {
        assert(!FreeIndices.empty());
        int idx = FreeIndices.back();
        FreeIndices.pop_back();
        out_cpu_desc_handle->ptr = HeapStartCpu.ptr + (idx * HeapHandleIncrement);
        out_gpu_desc_handle->ptr = HeapStartGpu.ptr + (idx * HeapHandleIncrement);
    }

    void Free(D3D12_CPU_DESCRIPTOR_HANDLE out_cpu_desc_handle, D3D12_GPU_DESCRIPTOR_HANDLE out_gpu_desc_handle)
    {
        int cpu_idx = (int)((out_cpu_desc_handle.ptr - HeapStartCpu.ptr) / HeapHandleIncrement);
        int gpu_idx = (int)((out_gpu_desc_handle.ptr - HeapStartGpu.ptr) / HeapHandleIncrement);
        assert(cpu_idx == gpu_idx);
        FreeIndices.push_back(cpu_idx);
    }

    ID3D12DescriptorHeap* GetDescriptorHeap() { return m_heap.Get(); }

private:
    ComPtr<ID3D12DescriptorHeap> m_heap;
    D3D12_CPU_DESCRIPTOR_HANDLE HeapStartCpu;
    D3D12_GPU_DESCRIPTOR_HANDLE HeapStartGpu;
    UINT                        HeapHandleIncrement;
    std::vector<UINT>           FreeIndices;
};