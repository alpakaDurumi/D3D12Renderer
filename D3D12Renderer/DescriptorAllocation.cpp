#include "pch.h"
#include "DescriptorAllocation.h"

#include "D3DHelper.h"
#include "DescriptorAllocatorPage.h"

using namespace D3DHelper;

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& other) noexcept
    : m_descriptor(other.m_descriptor),
    m_offsetInHeap(other.m_offsetInHeap),
    m_numHandles(other.m_numHandles),
    m_descriptorSize(other.m_descriptorSize),
    m_fenceValue(other.m_fenceValue),
    m_pPage(other.m_pPage)
{
    other.m_descriptor.ptr = 0;
    other.m_offsetInHeap = 0;
    other.m_numHandles = 0;
    other.m_descriptorSize = 0;
    other.m_fenceValue = 0;
    other.m_pPage = nullptr;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& other) noexcept
{
    if (this != &other)
    {
        Free();

        m_descriptor = other.m_descriptor;
        m_offsetInHeap = other.m_offsetInHeap;
        m_numHandles = other.m_numHandles;
        m_descriptorSize = other.m_descriptorSize;
        m_fenceValue = other.m_fenceValue;
        m_pPage = other.m_pPage;

        other.m_descriptor.ptr = 0;
        other.m_offsetInHeap = 0;
        other.m_numHandles = 0;
        other.m_descriptorSize = 0;
        other.m_fenceValue = 0;
        other.m_pPage = nullptr;
    }

    return *this;
}

DescriptorAllocation::DescriptorAllocation(
    D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor,
    UINT32 offsetInHeap,
    UINT32 numHandles,
    UINT32 descriptorSize,
    DescriptorAllocatorPage* pPage)
    : m_descriptor(GetCPUDescriptorHandle(baseDescriptor, offsetInHeap, descriptorSize)),
    m_offsetInHeap(offsetInHeap),
    m_numHandles(numHandles),
    m_descriptorSize(descriptorSize),
    m_fenceValue(0),
    m_pPage(pPage)
{
}

DescriptorAllocation::~DescriptorAllocation()
{
    Free();
}

void DescriptorAllocation::Free()
{
    if (m_pPage)
        m_pPage->Free(std::move(*this));
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetDescriptorHandle(UINT32 offsetInBlock) const
{
    assert(offsetInBlock < m_numHandles);
    return { m_descriptor.ptr + (m_descriptorSize * offsetInBlock) };
}