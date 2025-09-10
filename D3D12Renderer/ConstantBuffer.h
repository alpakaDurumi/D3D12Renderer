#pragma once

#include <wrl/client.h>
#include <d3d12.h>

using Microsoft::WRL::ComPtr;

template<typename T>
class ConstantBuffer
{
public:
    void Update(const T* pConstantData)
    {
        memcpy(m_pBufferBegin, pConstantData, sizeof(T));
    }

    ComPtr<ID3D12Resource> m_buffer;
    UINT8* m_pBufferBegin = nullptr;
};