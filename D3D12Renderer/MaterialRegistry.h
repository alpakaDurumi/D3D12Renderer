#pragma once

#include <unordered_map>

#include "Aliases.h"

class Material;

class MaterialRegistry
{
public:
    void Register(const MaterialHandle& handle, Material* pMesh)
    {
        m_hashMap[handle] = pMesh;
    }

    Material* Resolve(const MaterialHandle& handle) const
    {
        auto it = m_hashMap.find(handle);
        assert(it != m_hashMap.end());
        return it->second;
    }

private:
    std::unordered_map<MaterialHandle, Material*> m_hashMap;
};