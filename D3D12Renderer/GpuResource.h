#pragma once

#include <wrl/client.h>

#include <d3d12.h>

class GpuResource
{
public:
    ID3D12Resource* Get() const;
    void Reset();

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
};