#pragma once

#include <wrl/client.h>

#include <d3d12.h>

#include <deque>
#include <memory>

using Microsoft::WRL::ComPtr;

static const SIZE_T MAXPAGESIZE = 64 * 1024 * 1024;    // 64MB

// Helper class for managing intermediate resources for CPU-to-GPU data transfer.
class UploadBuffer
{
public:
    struct Allocation
    {
        void* CPUPtr;
        D3D12_GPU_VIRTUAL_ADDRESS GPUPtr;
        ID3D12Resource* pResource;
        SIZE_T Offset;
    };

    UploadBuffer(ComPtr<ID3D12Device10>& device, SIZE_T pageSize);

    SIZE_T GetPageSize() const { return m_pageSize; }
    Allocation Allocate(SIZE_T sizeInBytes, SIZE_T alignment);
    void Reset();


private:
    class Page
    {
    public:
        Page(ComPtr<ID3D12Device10>& device, SIZE_T sizeInBytes);
        ~Page();
        bool HasSpace(SIZE_T sizeInBytes, SIZE_T alignment) const;
        Allocation Allocate(SIZE_T sizeInBytes, SIZE_T alignment);
        void Reset();

    private:
        ComPtr<ID3D12Resource> m_resource;

        void* m_CPUBasePtr;
        D3D12_GPU_VIRTUAL_ADDRESS m_GPUBasePtr;

        SIZE_T m_pageSize;
        SIZE_T m_offset;
    };

    std::shared_ptr<Page> RequestPage();

    std::deque<std::shared_ptr<Page>> m_pagePool;           // ÀüÃ¼ Page
    std::deque<std::shared_ptr<Page>> m_availablePages;
    std::shared_ptr<Page> m_currentPage;
    SIZE_T m_pageSize;

    ComPtr<ID3D12Device10> m_device;
};
