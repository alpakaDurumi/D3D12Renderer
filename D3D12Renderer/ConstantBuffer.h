#pragma once

#include <wrl/client.h>
#include <d3d12.h>

#include "D3DHelper.h"

using Microsoft::WRL::ComPtr;

template<typename T>
class ConstantBuffer
{
public:
    ConstantBuffer(ComPtr<ID3D12Device>& device, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle)
    {
        CreateUploadHeap(device, sizeof(T), m_buffer);

        // Create constant buffer view
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_buffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = sizeof(T);

        device->CreateConstantBufferView(&cbvDesc, cpuHandle);

        // Do not unmap this until app close
        D3D12_RANGE readRange = { 0, 0 };
        ThrowIfFailed(m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pBufferBegin)));
    }

    void Update(const T* pConstantData)
    {
        memcpy(m_pBufferBegin, pConstantData, sizeof(T));
    }

    ComPtr<ID3D12Resource> m_buffer;
    UINT8* m_pBufferBegin = nullptr;
};