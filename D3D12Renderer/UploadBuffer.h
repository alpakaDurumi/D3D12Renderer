#pragma once

#include <wrl/client.h>
#include <d3d12.h>
#include <deque>
#include <memory>

using Microsoft::WRL::ComPtr;

class UploadBuffer
{
public:
    struct Allocation
    {
        void* CPU;
        D3D12_GPU_VIRTUAL_ADDRESS GPU;
    };

    UploadBuffer(ComPtr<ID3D12Device>& device, SIZE_T pageSize);

    SIZE_T GetPageSize() const { return m_pageSize; }
    Allocation Allocate(SIZE_T sizeInBytes, SIZE_T alignment);
    void Reset();


private:
    struct Page
    {
    public:
        Page(ComPtr<ID3D12Device>& device, SIZE_T sizeInBytes);
        ~Page();
        bool HasSpace(SIZE_T sizeInBytes, SIZE_T alignment) const;
        Allocation Allocate(SIZE_T sizeInBytes, SIZE_T alignment);
        void Reset();

    private:
        ComPtr<ID3D12Resource> m_resource;

        void* m_CPUPtr;
        D3D12_GPU_VIRTUAL_ADDRESS m_GPUPtr;

        SIZE_T m_pageSize;
        SIZE_T m_offset;
    };

    std::shared_ptr<Page> RequestPage();

    std::deque<std::shared_ptr<Page>> m_pagePool;
    std::deque<std::shared_ptr<Page>> m_availablePages;
    std::shared_ptr<Page> m_currentPage;
    SIZE_T m_pageSize;

    ComPtr<ID3D12Device> m_device;
};
