#pragma once

#include <unordered_map>
#include <cassert>

#include "Aliases.h"

class Mesh;

class MeshRegistry
{
public:
    void Register(const MeshHandle& handle, Mesh* pMesh)
    {
        m_hashMap[handle] = pMesh;
    }

    Mesh* Resolve(const MeshHandle& handle) const
    {
        auto it = m_hashMap.find(handle);
        assert(it != m_hashMap.end());
        return it->second;
    }

private:
    std::unordered_map<MeshHandle, Mesh*> m_hashMap;
};