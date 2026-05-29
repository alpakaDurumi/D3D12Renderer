#include "pch.h"

#include "ImGuiDescriptorAllocator.h"

#include "D3DHelper.h"
#include "ImGuiDescriptorAllocation.h"

using namespace D3DHelper;

void ImGuiDescriptorAllocator::Init(ID3D12Device* pDevice)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = NumDescriptors;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_heap)));

    m_baseCpuHandle = m_heap->GetCPUDescriptorHandleForHeapStart();
    m_baseGpuHandle = m_heap->GetGPUDescriptorHandleForHeapStart();
    m_handleIncrementSize = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    m_freeIndices.resize(NumDescriptors);
    for (UINT i = 0; i < NumDescriptors; ++i)
        m_freeIndices[i] = i;
}

ID3D12DescriptorHeap* ImGuiDescriptorAllocator::GetDescriptorHeap()
{
    return m_heap.Get();
}

void ImGuiDescriptorAllocator::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
    assert(!m_freeIndices.empty());
    UINT idx = m_freeIndices.back();
    m_freeIndices.pop_back();
    *outCpuHandle = GetCpuDescriptorHandle(m_baseCpuHandle, idx, m_handleIncrementSize);
    *outGpuHandle = GetGpuDescriptorHandle(m_baseGpuHandle, idx, m_handleIncrementSize);
}

ImGuiDescriptorAllocation ImGuiDescriptorAllocator::Allocate()
{
    assert(!m_freeIndices.empty());
    UINT idx = m_freeIndices.back();
    m_freeIndices.pop_back();

    return {GetCpuDescriptorHandle(m_baseCpuHandle, idx, m_handleIncrementSize),
            GetGpuDescriptorHandle(m_baseGpuHandle, idx, m_handleIncrementSize),
            this};
}

void ImGuiDescriptorAllocator::Free(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle)
{
    UINT cpuIdx = static_cast<UINT>((cpuHandle.ptr - m_baseCpuHandle.ptr) / m_handleIncrementSize);
    UINT gpuIdx = static_cast<UINT>((gpuHandle.ptr - m_baseGpuHandle.ptr) / m_handleIncrementSize);
    assert(cpuIdx == gpuIdx);
    m_freeIndices.push_back(cpuIdx);
}
