#pragma once

#include <Windows.h>

#include <vector>
#include <utility>
#include <cassert>

template<typename T>
class SlotMap
{
public:
    struct Handle
    {
        UINT index = UINT_MAX;
        UINT generation = 0;

        bool operator==(const Handle& other) const
        {
            return this->index == other.index &&
                this->generation == other.generation;
        }

        bool operator!=(const Handle& other) const
        {
            return !(*this == other);
        }
    };

    Handle Add(T&& obj)
    {
        UINT slotIdx;
        if (!m_freeList.empty())
        {
            slotIdx = m_freeList.back();
            m_freeList.pop_back();
        }
        else
        {
            slotIdx = (UINT)m_slots.size();
            m_slots.push_back({});
        }

        UINT denseIdx = (UINT)m_dense.size();
        m_dense.push_back(std::move(obj));
        m_reverseMap.push_back(slotIdx);

        m_slots[slotIdx].denseIndex = denseIdx;

        return { slotIdx, m_slots[slotIdx].generation };
    }

    void Remove(Handle handle)
    {
        assert(IsValid(handle));

        UINT idx = m_slots[handle.index].denseIndex;
        UINT lastIdx = (UINT)m_dense.size() - 1;

        if (idx != lastIdx)
        {
            // Move T and overwrite reverseMap/slot
            m_dense[idx] = std::move(m_dense[lastIdx]);
            m_reverseMap[idx] = m_reverseMap[lastIdx];
            m_slots[m_reverseMap[idx]].denseIndex = idx;
        }

        m_dense.pop_back();
        m_reverseMap.pop_back();

        m_slots[handle.index].generation++;
        m_freeList.push_back(handle.index);
    }

    T* Get(Handle handle)
    {
        if (!IsValid(handle)) return nullptr;
        return &m_dense[m_slots[handle.index].denseIndex];
    }

    bool IsValid(Handle handle) const
    {
        return handle.index < m_slots.size()
            && m_slots[handle.index].generation == handle.generation;
    }

    std::vector<T>& GetDense() { return m_dense; }
    const std::vector<T>& GetDense() const { return m_dense; }

    UINT GetCount() const
    {
        return static_cast<UINT>(m_dense.size());
    }

    UINT GetDenseIndex(Handle handle) const
    {
        assert(IsValid(handle));
        return m_slots[handle.index].denseIndex;
    }

private:
    struct Slot
    {
        UINT denseIndex;
        UINT generation = 0;
    };

    std::vector<T> m_dense;
    std::vector<UINT> m_reverseMap;  // denseIdx → slotIdx
    std::vector<Slot> m_slots;
    std::vector<UINT> m_freeList;
};