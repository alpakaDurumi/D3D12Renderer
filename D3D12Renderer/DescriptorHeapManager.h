#pragma once

#include <wrl/client.h>
#include <d3d12.h>

#include "D3DHelper.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

// D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV 타입의 디스크립터 힙을 관리하기 위한 클래스
class DescriptorHeapManager
{
public:
    void Init(ComPtr<ID3D12Device10>& device, UINT frameCount, UINT maxCbvPerFrame, UINT maxSrv)
    {
        D3D12_DESCRIPTOR_HEAP_DESC cbvSrvUavDesc = {};
        cbvSrvUavDesc.NumDescriptors = (frameCount * maxCbvPerFrame) + maxSrv;
        cbvSrvUavDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvSrvUavDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(device->CreateDescriptorHeap(&cbvSrvUavDesc, IID_PPV_ARGS(&m_heap)));

        m_descriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        m_srvStart = frameCount * maxCbvPerFrame;
        m_cpuHeapStart = m_heap->GetCPUDescriptorHandleForHeapStart();
        m_gpuHeapStart = m_heap->GetGPUDescriptorHandleForHeapStart();
    }

    UINT GetNumCbvAllocated()
    {
        return m_numCbvAllocated;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetFreeHandleForCbv()
    {
        D3D12_CPU_DESCRIPTOR_HANDLE temp = m_cpuHeapStart;
        MoveCPUDescriptorHandle(&temp, m_numCbvAllocated, m_descriptorSize);

        m_numCbvAllocated++;

        return temp;
    }

    UINT GetNumSrvAllocated()
    {
        return m_numSrvAllocated;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetFreeHandleForSrv()
    {
        D3D12_CPU_DESCRIPTOR_HANDLE temp = m_cpuHeapStart;
        MoveCPUDescriptorHandle(&temp, m_srvStart + m_numSrvAllocated, m_descriptorSize);

        m_numSrvAllocated++;

        return temp;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetCbvHandle(UINT frameIndex, UINT cbvIndex)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE temp = m_gpuHeapStart;
        MoveGPUDescriptorHandle(&temp, frameIndex * m_numCbvPerFrame + cbvIndex, m_descriptorSize);

        return temp;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle(UINT srvIndex)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE temp = m_gpuHeapStart;
        MoveGPUDescriptorHandle(&temp, m_srvStart + srvIndex, m_descriptorSize);

        return temp;
    }

    ComPtr<ID3D12DescriptorHeap> m_heap;
    UINT m_descriptorSize;
    UINT m_srvStart;                                // 힙 내에서 SRV가 시작되는 부분
    UINT m_numCbvPerFrame = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE m_cpuHeapStart;
    D3D12_GPU_DESCRIPTOR_HANDLE m_gpuHeapStart;

    UINT m_numCbvAllocated = 0;
    UINT m_numSrvAllocated = 0;
};