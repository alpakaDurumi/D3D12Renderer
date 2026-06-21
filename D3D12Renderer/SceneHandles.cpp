#include "pch.h"

#include "SceneHandles.h"

std::size_t std::hash<MeshHandle>::operator()(const MeshHandle& h) const
{
    return (static_cast<UINT64>(h.index) << 32) | static_cast<UINT64>(h.generation);
}

std::size_t std::hash<EntityHandle>::operator()(const EntityHandle& h) const
{
    return (static_cast<UINT64>(h.index) << 32) | static_cast<UINT64>(h.generation);
}
