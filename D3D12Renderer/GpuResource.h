#pragma once

#include <wrl/client.h>

#include <d3d12.h>

class GpuResource
{
public:
    GpuResource() = default;

    GpuResource(const GpuResource&) = delete;
    GpuResource& operator=(const GpuResource&) = delete;
    GpuResource(GpuResource&&) = default;
    GpuResource& operator=(GpuResource&&) = default;

    explicit GpuResource(Microsoft::WRL::ComPtr<ID3D12Resource>&& existing);
    ~GpuResource() = default;

    ID3D12Resource* Get() const;
    void Reset();

protected:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_resource;
};