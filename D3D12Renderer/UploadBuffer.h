#pragma once

#include <wrl/client.h>

#include <d3d12.h>

#include <memory>
#include <queue>

class CommandQueue;

using Microsoft::WRL::ComPtr;

// Helper class for managing intermediate resources for CPU-to-GPU data transfer.
class UploadBuffer
{
public:
    // Disable copy and move. Only use as l-value reference
    UploadBuffer(const UploadBuffer&) = delete;
    UploadBuffer& operator=(const UploadBuffer&) = delete;
    UploadBuffer(UploadBuffer&&) = delete;
    UploadBuffer& operator=(UploadBuffer&&) = delete;

    struct Allocation
    {
        // Disable copy
        Allocation(const Allocation&) = delete;
        Allocation& operator=(const Allocation&) = delete;

        Allocation(Allocation&& other) noexcept;
        Allocation& operator=(Allocation&& other) noexcept;

        Allocation(void* cpuPtr, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr, ID3D12Resource* pResource, SIZE_T offset)
            : CPUPtr(cpuPtr), GPUPtr(gpuPtr), pResource(pResource), Offset(offset)
        {
        }

        void* CPUPtr;
        D3D12_GPU_VIRTUAL_ADDRESS GPUPtr;
        ID3D12Resource* pResource;
        SIZE_T Offset;
    };

    UploadBuffer(const ComPtr<ID3D12Device10>& device, const CommandQueue& commandQueue, SIZE_T pageSize);

    SIZE_T GetPageSize() const { return m_pageSize; }
    Allocation Allocate(SIZE_T sizeInBytes, SIZE_T alignment);
    void QueueRetiredPages(UINT64 fenceValue);
private:
    class Page
    {
    public:
        Page(ID3D12Device10* pDevice, SIZE_T sizeInBytes);
        ~Page();

        ComPtr<ID3D12Resource> m_resource;
        void* m_CPUBasePtr;
        D3D12_GPU_VIRTUAL_ADDRESS m_GPUBasePtr;
        SIZE_T m_pageSize;
    };

    Page* RequestPage();
 
    std::vector<std::unique_ptr<Page>> m_pagePool;          // Pool containing all pages
    std::queue<Page*> m_availablePages;                     // List of pages immediately available for use
    std::vector<Page*> m_retiredPages;                      // Pages that ran out of space during a single command list recording
    std::queue<std::pair<UINT64, Page*>> m_pendingPages;    // Queue of pages to be reused when work is completed, compared against given fenceValues

    Page* m_currentPage;
    SIZE_T m_currentOffset;

    SIZE_T m_pageSize;

    ComPtr<ID3D12Device10> m_device;
    const CommandQueue& m_commandQueue;     // For IsFenceComplete
};
