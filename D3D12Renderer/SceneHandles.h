#pragma once

#include <cstddef>
#include <functional>

#include "SlotMap.h"

class Mesh;
class Material;
class DirectionalLight;
class PointLight;
class SpotLight;
struct AssetTexture;
struct Entity;

using MeshHandle = SlotMap<Mesh>::Handle;
using MaterialHandle = SlotMap<Material>::Handle;
using DirectionalLightHandle = SlotMap<DirectionalLight>::Handle;
using PointLightHandle = SlotMap<PointLight>::Handle;
using SpotLightHandle = SlotMap<SpotLight>::Handle;
using AssetTextureHandle = SlotMap<AssetTexture>::Handle;
using EntityHandle = SlotMap<Entity>::Handle;

template <>
struct std::hash<MeshHandle>
{
    std::size_t operator()(const MeshHandle& h) const;
};

template <>
struct std::hash<EntityHandle>
{
    std::size_t operator()(const EntityHandle& h) const;
};
