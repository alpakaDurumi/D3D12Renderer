#include "pch.h"
#include "DescriptorAllocation.h"

#include "D3DHelper.h"
#include "DescriptorAllocatorPage.h"

using namespace D3DHelper;

DescriptorAllocation::DescriptorAllocation()
    : m_descriptor{ 0 },
    m_offsetInHeap(0),
    m_numHandles(0),
    m_descriptorSize(0),
    m_fenceValue(0),
    m_pPage(nullptr)
{
}

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

bool DescriptorAllocation::IsNull() const
{
    return m_pPage == nullptr;
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

std::vector<DescriptorAllocation> DescriptorAllocation::Split()
{
    assert(!IsNull());

    std::vector<DescriptorAllocation> ret;
    ret.reserve(m_numHandles);

    for (UINT i = 0; i < m_numHandles; ++i)
    {
        auto descriptor = GetCPUDescriptorHandle(m_descriptor, i, m_descriptorSize);

        // Can't use emplace_back here :
        // Required constructor is private and emplace_back delegates construction to std::allocator class,
        // which cannot access private constructor.
        ret.push_back(DescriptorAllocation(
            descriptor,
            m_offsetInHeap + i,
            1,
            m_descriptorSize,
            m_fenceValue,
            m_pPage));
    }

    m_descriptor.ptr = 0;
    m_offsetInHeap = 0;
    m_numHandles = 0;
    m_descriptorSize = 0;
    m_fenceValue = 0;
    m_pPage = nullptr;

    return ret;
}

// private constructor only for Split function.
DescriptorAllocation::DescriptorAllocation(
    D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
    UINT32 offsetInHeap,
    UINT32 numHandles,
    UINT32 descriptorSize,
    UINT64 fenceValue,
    DescriptorAllocatorPage* pPage)
    : m_descriptor(descriptor),
    m_offsetInHeap(offsetInHeap),
    m_numHandles(numHandles),
    m_descriptorSize(descriptorSize),
    m_fenceValue(fenceValue),
    m_pPage(pPage)
{
}