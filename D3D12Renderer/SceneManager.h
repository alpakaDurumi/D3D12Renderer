#pragma once

#include <cassert>
#include <optional>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include <DirectXCollision.h>
#include <DirectXMath.h>
#include <basetsd.h>
#include <d3d12.h>
#include <minwindef.h>

#include "Aliases.h"
#include "GpuResource.h"
#include "InstanceData.h"
#include "Light.h"
#include "Material.h"
#include "Mesh.h"
#include "SceneHandles.h"
#include "SlotMap.h"
#include "Texture.h"
#include "Transform.h"
#include "View.h"

struct GeometryData;
class TransientUploadAllocator;
class DescriptorAllocation;

struct InstanceRange
{
    UINT baseIndex;
    UINT forwardCount;
    UINT deferredCount;
};

using LightHandle = std::variant<
    DirectionalLightHandle,
    PointLightHandle,
    SpotLightHandle>;

struct MeshRenderer
{
    MeshHandle mesh;
    MaterialHandle material;
};

struct Entity
{
    std::string name;

    EntityHandle selfHandle;
    EntityHandle parent;
    std::vector<EntityHandle> children;

    Transform transform;
    std::optional<MeshRenderer> meshRenderer;
    std::optional<LightHandle> light;
};

struct AssetTexture
{
    Texture texture;
    ShaderResourceView srv;
};

class SceneManager
{
public:
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;

    SceneManager() = default;
    ~SceneManager() = default;

    void Init(ID3D12Device10* pDevice)
    {
        m_pDevice = pDevice;
    }

    // Entity
    EntityHandle AddEntity(const std::string& name);
    void Remove(EntityHandle handle);
    void AddChild(EntityHandle parent, EntityHandle child);

    Entity* Get(EntityHandle h)
    {
        return m_entities.Get(h);
    }

    const std::vector<Entity>& GetEntities() const
    {
        return m_entities.GetDense();
    }

    std::vector<Entity>& GetEntities()
    {
        return m_entities.GetDense();
    }

    void AddComponent(EntityHandle eh, LightHandle lh)
    {
        auto* pEntity = m_entities.Get(eh);
        pEntity->light = lh;
    }

    void ApplyTransform(EntityHandle eh, const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& eulerRad, const DirectX::XMFLOAT3& t)
    {
        auto* pEntity = m_entities.Get(eh);
        pEntity->transform.Apply(s, eulerRad, t);
    }

    void SetMesh(EntityHandle eh, MeshHandle mh);
    void SetMaterial(EntityHandle eh, MaterialHandle mh);

    void SetForward(EntityHandle eh, const DirectX::XMFLOAT3& forward)
    {
        auto* pEntity = m_entities.Get(eh);
        pEntity->transform.SetForward(forward);
    }

    // Mesh
    MeshHandle AddMesh(
        ID3D12GraphicsCommandList7* pCommandList,
        TransientUploadAllocator& allocator,
        const GeometryData& data);

    Mesh* GetMesh(MeshHandle handle)
    {
        return m_meshes.Get(handle);
    }

    MeshHandle GetMeshHandle(const AssetID& id) const
    {
        auto it = m_meshRegistry.find(id);
        assert(it != m_meshRegistry.end());

        return it->second;
    }

    void RegisterMesh(MeshHandle handle, const AssetID& id)
    {
        m_meshRegistry[id] = handle;
    }

    // Material
    MaterialHandle AddMaterial(DescriptorAllocation&& allocation)
    {
        return m_materials.Add(Material(std::move(allocation)));
    }

    MaterialHandle AddMaterial(DescriptorAllocation&& allocation, const AssetID& id)
    {
        auto handle = m_materials.Add(Material(std::move(allocation)));
        m_materialRegistry[id] = handle;
        return handle;
    }

    Material* GetMaterial(MaterialHandle handle)
    {
        return m_materials.Get(handle);
    }

    MaterialHandle GetMaterialHandle(const AssetID& id) const
    {
        auto it = m_materialRegistry.find(id);
        assert(it != m_materialRegistry.end());

        return it->second;
    }

    void RegisterMaterial(MaterialHandle handle, const AssetID& id)
    {
        m_materialRegistry[id] = handle;
    }

    const std::vector<Material>& GetMaterials() const
    {
        return m_materials.GetDense();
    }

    std::vector<Material>& GetMaterials()
    {
        return m_materials.GetDense();
    }

    // Light
    DirectionalLightHandle AddDirectionalLight(
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        UINT shadowMapResolution)
    {
        return m_directionalLights.Add(DirectionalLight(
            m_pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            std::move(cbvAllocation),
            shadowMapResolution));
    }

    PointLightHandle AddPointLight(
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        DescriptorAllocation&& rtvAllocation,
        UINT shadowMapResolution)
    {
        return m_pointLights.Add(PointLight(
            m_pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            std::move(cbvAllocation),
            std::move(rtvAllocation),
            shadowMapResolution));
    }

    SpotLightHandle AddSpotLight(
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        UINT shadowMapResolution)
    {
        return m_spotLights.Add(SpotLight(
            m_pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            std::move(cbvAllocation),
            shadowMapResolution));
    }

    DirectionalLight* Get(DirectionalLightHandle h)
    {
        return m_directionalLights.Get(h);
    }

    PointLight* Get(PointLightHandle h)
    {
        return m_pointLights.Get(h);
    }

    SpotLight* Get(SpotLightHandle h)
    {
        return m_spotLights.Get(h);
    }

    const std::vector<DirectionalLight>& GetDirectionalLights() const
    {
        return m_directionalLights.GetDense();
    }

    std::vector<DirectionalLight>& GetDirectionalLights()
    {
        return m_directionalLights.GetDense();
    }

    const std::vector<PointLight>& GetPointLights() const
    {
        return m_pointLights.GetDense();
    }

    std::vector<PointLight>& GetPointLights()
    {
        return m_pointLights.GetDense();
    }

    const std::vector<SpotLight>& GetSpotLights() const
    {
        return m_spotLights.GetDense();
    }

    std::vector<SpotLight>& GetSpotLights()
    {
        return m_spotLights.GetDense();
    }

    UINT GetLightCount() const
    {
        return m_directionalLights.GetCount() + m_pointLights.GetCount() + m_spotLights.GetCount();
    }

    UINT GetShadowMapResolution(LightHandle handle);
    void SetShadowMapResolution(LightHandle handle, UINT resolution);

    // Asset texture
    AssetTextureHandle AddAssetTexture(
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& srvAllocation,
        TransientUploadAllocator& uploadAllocator,
        const std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height);

    AssetTextureHandle AddAssetTexture(
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& srvAllocation,
        TransientUploadAllocator& uploadAllocator,
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage,
        bool isCubeMap);

    const std::vector<AssetTexture>& GetAssetTextures() const
    {
        return m_assetTextures.GetDense();
    }

    std::vector<AssetTexture>& GetAssetTextures()
    {
        return m_assetTextures.GetDense();
    }

    // Per-frame
    void UpdateWorldTransforms(float alpha);
    std::vector<InstanceData> GatherInstances();

    const std::unordered_map<MeshHandle, InstanceRange>& GetInstanceRanges() const
    {
        return m_instanceRanges;
    }

    UINT GetEntityIndex(EntityHandle entity) const
    {
        auto it = m_entityIndex.find(entity);
        assert(it != m_entityIndex.end());

        return it->second;
    }

    const std::vector<DirectX::BoundingSphere>& GetWorldBoundingSpheres() const
    {
        return m_worldBoundingSpheres;
    }

    // Deferred deletion
    void EnqueueResourceDeletion(std::vector<GpuResource>& resources);
    void QueueDeferredDeletions(UINT64 signaledFenceValue);
    void ProcessCompletedDeletions(UINT64 completedFenceValue);

private:
    struct MeshBucket
    {
        std::vector<InstanceData> forward;
        std::vector<InstanceData> deferred;
    };

    struct DeferredResource
    {
        DeferredResource(UINT64 fenceValue, GpuResource&& resource)
            : fenceValue(fenceValue)
            , resource(std::move(resource))
        {
        }

        UINT64 fenceValue;
        GpuResource resource;
    };

    void Remove(DirectionalLightHandle handle)
    {
        m_directionalLights.Remove(handle);
    }

    void Remove(PointLightHandle handle)
    {
        m_pointLights.Remove(handle);
    }

    void Remove(SpotLightHandle handle)
    {
        m_spotLights.Remove(handle);
    }

    void XM_CALLCONV UpdateWorldTransform(Entity& entity, DirectX::FXMMATRIX accumulated, float alpha);

    SlotMap<Mesh> m_meshes;
    std::unordered_map<AssetID, MeshHandle> m_meshRegistry;

    std::unordered_map<MeshHandle, MeshBucket> m_buckets;

    std::unordered_map<MeshHandle, InstanceRange> m_instanceRanges;

    std::unordered_map<EntityHandle, UINT> m_entityIndex;

    std::vector<DirectX::BoundingSphere> m_worldBoundingSpheres;

    SlotMap<Material> m_materials;
    std::unordered_map<AssetID, MaterialHandle> m_materialRegistry;

    SlotMap<DirectionalLight> m_directionalLights;
    SlotMap<PointLight> m_pointLights;
    SlotMap<SpotLight> m_spotLights;

    SlotMap<AssetTexture> m_assetTextures;

    SlotMap<Entity> m_entities;

    std::vector<GpuResource> m_deferred; // List of resources requested to be removed

    std::queue<DeferredResource> m_deletionQueue;

    ID3D12Device10* m_pDevice = nullptr;
};
