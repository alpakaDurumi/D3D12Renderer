#pragma once

#include <unordered_map>
#include <cassert>

#include "Aliases.h"

class Mesh;

class MeshRegistry
{
public:
    void Register(const MeshName& name, Mesh* pMesh)
    {
        m_hashMap[name] = pMesh;
    }

    Mesh* Resolve(const MeshName& name) const
    {
        auto it = m_hashMap.find(name);
        assert(it != m_hashMap.end());
        return it->second;
    }

private:
    std::unordered_map<MeshName, Mesh*> m_hashMap;
};