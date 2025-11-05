#pragma once

#include <wrl/client.h>

#include <d3d12.h>

#include <memory>
#include <queue>

class CommandQueue;

using Microsoft::WRL::ComPtr;

static const SIZE_T MAXPAGESIZE = 64 * 1024 * 1024;    // 64MB

// Helper class for managing intermediate resources for CPU-to-GPU data transfer.
class UploadBuffer
{
public:
    struct Allocation
    {
        //// Disable copy
        //Allocation(const Allocation&) = delete;
        //Allocation& operator=(const Allocation&) = delete;

        Allocation(void* cpuPtr, D3D12_GPU_VIRTUAL_ADDRESS gpuPtr, ID3D12Resource* pResource, SIZE_T offset)
            : CPUPtr(cpuPtr), GPUPtr(gpuPtr), pResource(pResource), Offset(offset)
        {
        }

        void* CPUPtr;
        D3D12_GPU_VIRTUAL_ADDRESS GPUPtr;
        ID3D12Resource* pResource;
        SIZE_T Offset;
    };

    UploadBuffer(ComPtr<ID3D12Device10>& device, const CommandQueue& commandQueue, SIZE_T pageSize);

    SIZE_T GetPageSize() const { return m_pageSize; }
    Allocation Allocate(SIZE_T sizeInBytes, SIZE_T alignment);
    void QueueRetiredPages(UINT64 fenceValue);
private:
    class Page
    {
    public:
        Page(ID3D12Device10* pDevice, SIZE_T sizeInBytes);
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

    Page* RequestPage();
 
    std::vector<std::unique_ptr<Page>> m_pagePool;          // 전체 Page를 담는 Pool
    std::queue<Page*> m_availablePages;                     // 즉시 사용 가능한 Page 목록
    std::vector<Page*> m_retiredPages;                      // 한 번의 command list 작성 중, 더 이상 할당할 공간이 없게 된 page 목록
    std::queue<std::pair<Page*, UINT64>> m_pendingPages;    // 주어진 fenceValue와 비교하여 작업이 완료된 page를 꺼내 쓰게되는 목록

    Page* m_currentPage;
    SIZE_T m_pageSize;

    ComPtr<ID3D12Device10> m_device;
    const CommandQueue& m_commandQueue;     // For IsFenceComplete
};
