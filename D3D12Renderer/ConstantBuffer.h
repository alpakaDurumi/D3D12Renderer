#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>

#include <optional>
#include <utility>

#include "D3DHelper.h"
#include "DescriptorAllocation.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

template<typename T>
class ConstantBuffer
{
public:
    // Consturctor for root descriptor
    ConstantBuffer(ID3D12Device10* pDevice)
        : m_allocation(std::nullopt)
    {
        CreateUploadHeap(pDevice, sizeof(T), m_buffer);

        D3D12_RANGE readRange = { 0, 0 };
        ThrowIfFailed(m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pBufferBegin)));
    }

    // Consturctor for descriptor table
    ConstantBuffer(ID3D12Device10* pDevice, DescriptorAllocation&& allocation)
        : m_allocation(std::move(allocation))
    {
        assert(!m_allocation->IsNull());

        CreateUploadHeap(pDevice, sizeof(T), m_buffer);

        // Create constant buffer view
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = m_buffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = sizeof(T);
        pDevice->CreateConstantBufferView(&cbvDesc, m_allocation->GetDescriptorHandle());

        D3D12_RANGE readRange = { 0, 0 };
        ThrowIfFailed(m_buffer->Map(0, &readRange, reinterpret_cast<void**>(&m_pBufferBegin)));
    }

    ~ConstantBuffer()
    {
        m_buffer->Unmap(0, nullptr);
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const
    {
        return m_buffer->GetGPUVirtualAddress();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorHandle() const
    {
        return m_allocation->GetDescriptorHandle();
    }

    void Update(const T* pConstantData)
    {
        memcpy(m_pBufferBegin, pConstantData, sizeof(T));
    }

private:
    ComPtr<ID3D12Resource> m_buffer;
    UINT8* m_pBufferBegin = nullptr;
    std::optional<DescriptorAllocation> m_allocation;   // Only valid when ConstantBuffer is used for descriptor table
};