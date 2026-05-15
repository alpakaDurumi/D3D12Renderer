#pragma once

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