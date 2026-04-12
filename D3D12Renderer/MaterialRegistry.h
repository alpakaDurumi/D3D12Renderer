#pragma once

#include <unordered_map>
#include <cassert>

#include "Aliases.h"

class Material;

class MaterialRegistry
{
public:
    void Register(const MaterialName& name, Material* pMat)
    {
        m_hashMap[name] = pMat;
    }

    Material* Resolve(const MaterialName& name) const
    {
        auto it = m_hashMap.find(name);
        assert(it != m_hashMap.end());
        return it->second;
    }

private:
    std::unordered_map<MaterialName, Material*> m_hashMap;
};