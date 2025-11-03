#include "DescriptorAllocation.h"

#include <cassert>

#include "D3DHelper.h"
#include "DescriptorAllocatorPage.h"

using namespace D3DHelper;

DescriptorAllocation::DescriptorAllocation(D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor, UINT32 offsetInHeap, UINT32 numHandles, UINT32 descriptorSize, std::shared_ptr<DescriptorAllocatorPage> page)
    : m_offsetInHeap(offsetInHeap), m_numHandles(numHandles), m_descriptorSize(descriptorSize), m_fenceValue(0), m_page(page)
{
    m_descriptor = baseDescriptor;
    MoveCPUDescriptorHandle(&m_descriptor, m_offsetInHeap, descriptorSize);
}

DescriptorAllocation::~DescriptorAllocation()
{
    Free();
}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& other)
    : m_descriptor(other.m_descriptor), m_offsetInHeap(other.m_offsetInHeap), m_numHandles(other.m_numHandles), m_descriptorSize(other.m_descriptorSize), m_fenceValue(other.m_fenceValue), m_page(std::move(other.m_page))
{
    other.m_descriptor.ptr = 0;
    other.m_offsetInHeap = 0;
    other.m_numHandles = 0;
    other.m_descriptorSize = 0;
    other.m_fenceValue = 0;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& other)
{
    Free();

    m_descriptor = other.m_descriptor;
    m_offsetInHeap = other.m_offsetInHeap;
    m_numHandles = other.m_numHandles;
    m_descriptorSize = other.m_descriptorSize;
    m_fenceValue = other.m_fenceValue;
    m_page = std::move(other.m_page);

    other.m_descriptor.ptr = 0;
    other.m_offsetInHeap = 0;
    other.m_numHandles = 0;
    other.m_descriptorSize = 0;
    other.m_fenceValue = 0;

    return *this;
}

void DescriptorAllocation::Free()
{
    if(m_page)
        m_page->Free(std::move(*this));
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocation::GetDescriptorHandle(UINT32 offsetInBlock) const
{
    assert(offsetInBlock < m_numHandles);
    return { m_descriptor.ptr + (m_descriptorSize * offsetInBlock) };
}