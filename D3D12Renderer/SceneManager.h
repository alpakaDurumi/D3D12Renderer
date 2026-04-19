#pragma once

#include <unordered_map>
#include <variant>
#include <cassert>

#include "SlotMap.h"
#include "Mesh.h"
#include "Material.h"
#include "Light.h"
#include "Texture.h"
#include "Aliases.h"
#include "InstanceData.h"
#include "SceneHandles.h"
#include "Transform.h"

template<>
struct std::hash<MeshHandle>
{
    std::size_t operator()(const MeshHandle& h) const
    {
        return (static_cast<UINT64>(h.index) << 32) | static_cast<UINT64>(h.generation);
    }
};

struct InstanceRange
{
    UINT offset;            // offset in instance buffer
    UINT forwardCount;
    UINT deferredCount;
};

struct MeshBucket
{
    std::vector<InstanceData> forward;
    std::vector<InstanceData> deferred;
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

    std::optional<Transform> transform;
    std::optional<MeshRenderer> meshRenderer;
    std::optional<LightHandle> light;
};

class SceneManager
{
public:
    EntityHandle AddEntity(const std::string& name)
    {
        auto handle = m_entities.Add(Entity());

        auto* pEntity = m_entities.Get(handle);
        pEntity->name = name;
        pEntity->selfHandle = handle;

        return handle;
    }

    void Remove(EntityHandle handle)
    {
        if (!m_entities.IsValid(handle)) return;

        auto* pEntity = m_entities.Get(handle);

        // Should delete material with 0 usage?

        if (pEntity->light.has_value())
            std::visit([&](auto&& handle) { Remove(handle); }, pEntity->light.value());

        // Recursively Remove children entities
        auto childrenCopy = pEntity->children;
        for (auto child : childrenCopy)
            Remove(child);

        // If it have parent, remove handle from parent's children
        auto* pParent = m_entities.Get(pEntity->parent);
        if (pParent)
        {
            auto& children = pParent->children;
            children.erase(std::remove(children.begin(), children.end(), handle));
        }

        m_entities.Remove(handle);
    }

    void AddChild(EntityHandle parent, EntityHandle child)
    {
        auto* pParent = m_entities.Get(parent);
        auto* pChild = m_entities.Get(child);

        pParent->children.push_back(child);
        pChild->parent = parent;
    }

    void AddTransform(EntityHandle eh)
    {
        auto* pEntity = m_entities.Get(eh);
        if (pEntity->transform.has_value()) assert(false);

        pEntity->transform.emplace();
    }

    void AddTransform(EntityHandle eh, const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
    {
        auto* pEntity = m_entities.Get(eh);
        if (pEntity->transform.has_value()) assert(false);

        pEntity->transform.emplace(s, eulerRad, t);
    }

    void ApplyTransform(EntityHandle eh, const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
    {
        auto* pEntity = m_entities.Get(eh);
        if (!pEntity->transform.has_value()) assert(false);
        pEntity->transform->Apply(s, eulerRad, t);
    }

    void SetMesh(EntityHandle eh, MeshHandle mh)
    {
        auto* pEntity = m_entities.Get(eh);

        if (pEntity->meshRenderer.has_value())
        {
            pEntity->meshRenderer->mesh = mh;
        }
        else
        {
            pEntity->meshRenderer = { mh, GetMaterialHandle("builtin://material/default") };
        }
    }

    void SetMaterial(EntityHandle eh, MaterialHandle mh)
    {
        auto* pEntity = m_entities.Get(eh);

        if (pEntity->meshRenderer.has_value())
        {
            pEntity->meshRenderer->material = mh;
        }
        else
        {
            assert(false);
        }
    }

    void AddComponent(EntityHandle eh, LightHandle lh)
    {
        auto* pEntity = m_entities.Get(eh);
        pEntity->light = lh;
    }

    const std::vector<Entity>& GetEntities() const
    {
        return m_entities.GetDense();
    }

    std::vector<Entity>& GetEntities()
    {
        return m_entities.GetDense();
    }

    MeshHandle AddMesh(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        TransientUploadAllocator& allocator,
        const GeometryData& data)
    {
        auto handle = m_meshes.Add(Mesh(pDevice, pCommandList, allocator, data));
        m_meshRegistry[data.name] = handle;
        GetMesh(handle)->SetMaterial(GetMaterialHandle("builtin://material/default"));
        return handle;
    }

    Mesh* GetMesh(MeshHandle handle)
    {
        return m_meshes.Get(handle);
    }

    MeshHandle GetMeshHandle(const AssetID& id)
    {
        return m_meshRegistry[id];
    }

    void RegisterMesh(MeshHandle handle, const AssetID& id)
    {
        m_meshRegistry[id] = handle;
    }

    // Material
    MaterialHandle AddMaterial(ID3D12Device10* pDevice, DescriptorAllocation&& allocation)
    {
        return m_materials.Add(Material(pDevice, std::move(allocation)));
    }

    MaterialHandle AddMaterial(ID3D12Device10* pDevice, DescriptorAllocation&& allocation, const AssetID& id)
    {
        auto handle = m_materials.Add(Material(pDevice, std::move(allocation)));
        m_materialRegistry[id] = handle;
        return handle;
    }

    Material* GetMaterial(MaterialHandle handle)
    {
        return m_materials.Get(handle);
    }

    MaterialHandle GetMaterialHandle(const AssetID& id)
    {
        return m_materialRegistry[id];
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

    InstanceData BuildInstanceData(const XMFLOAT4X4& transform, UINT matIdx) const
    {
        InstanceData ret;

        auto world = XMLoadFloat4x4(&transform);

        XMStoreFloat4x4(&ret.world, XMMatrixTranspose(world));

        world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMStoreFloat4x4(&ret.inverseTranspose, XMMatrixInverse(nullptr, world));

        ret.materialIndex = matIdx;

        return ret;
    }

    std::vector<InstanceData> GatherInstances()
    {
        for (auto& [mesh, bucket] : m_buckets)
        {
            bucket.forward.clear();
            bucket.deferred.clear();
        }
        m_instanceRanges.clear();

        for (const auto& entity : m_entities.GetDense())
        {
            if (!entity.transform.has_value() && !entity.meshRenderer.has_value()) continue;

            auto meshHandle = entity.meshRenderer->mesh;
            auto matHandle = entity.meshRenderer->material;

            auto matIdx = m_materials.GetDenseIndex(matHandle);
            auto data = BuildInstanceData(entity.transform->GetRenderTransform(), matIdx);

            auto renderingPath = GetMaterial(matHandle)->GetRenderingPath();

            if (renderingPath == RenderingPath::FORWARD)
                m_buckets[meshHandle].forward.push_back(data);
            else
                m_buckets[meshHandle].deferred.push_back(data);
        }

        UINT currentOffset = 0;

        // InstanceData도 버켓에 담긴 순으로 flat array만들고, 드로우 콜도 그냥 버켓 순으로 해버리기?

        std::vector<InstanceData> temp;

        for (const auto& [meshHandle, bucket] : m_buckets)
        {
            const auto& [forward, deferred] = bucket;

            InstanceRange& range = m_instanceRanges[meshHandle];
            range.offset = currentOffset;
            range.forwardCount = static_cast<UINT>(forward.size());
            range.deferredCount = static_cast<UINT>(deferred.size());

            temp.insert(temp.end(), forward.begin(), forward.end());
            temp.insert(temp.end(), deferred.begin(), deferred.end());

            currentOffset += (range.forwardCount + range.deferredCount) * sizeof(InstanceData);
        }

        return temp;
    }

    InstanceRange GetInstanceRange(MeshHandle mesh)
    {
        return m_instanceRanges[mesh];
    }

    const std::unordered_map<MeshHandle, MeshBucket>& GetBuckets() const
    {
        return m_buckets;
    }

    DirectionalLightHandle AddDirectionalLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        UINT shadowMapResolution)
    {
        return m_directionalLights.Add(DirectionalLight(
            pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            std::move(cbvAllocation),
            shadowMapResolution));
    }

    PointLightHandle AddPointLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        DescriptorAllocation&& rtvAllocation,
        UINT shadowMapResolution)
    {
        return m_pointLights.Add(PointLight(
            pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            std::move(cbvAllocation),
            std::move(rtvAllocation),
            shadowMapResolution));
    }

    SpotLightHandle AddSpotLight(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& dsvAllocation,
        DescriptorAllocation&& srvAllocation,
        DescriptorAllocation&& cbvAllocation,
        UINT shadowMapResolution)
    {
        return m_spotLights.Add(SpotLight(
            pDevice,
            std::move(dsvAllocation),
            std::move(srvAllocation),
            std::move(cbvAllocation),
            shadowMapResolution));
    }

    Entity* Get(EntityHandle h)
    {
        return m_entities.Get(h);
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

    TextureHandle AddTexture(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& allocation,
        TransientUploadAllocator& uploadAllocator,
        const std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height)
    {
        return m_textures.Add(Texture(
            pDevice,
            pCommandList,
            std::move(allocation),
            uploadAllocator,
            textureSrc,
            width,
            height));
    }

    TextureHandle AddTexture(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& allocation,
        TransientUploadAllocator& uploadAllocator,
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage,
        bool isCubeMap)
    {
        return m_textures.Add(Texture(
            pDevice,
            pCommandList,
            std::move(allocation),
            uploadAllocator,
            filePath,
            isSRGB,
            useBlockCompress,
            flipImage,
            isCubeMap));
    }

    const std::vector<Texture>& GetTextures() const
    {
        return m_textures.GetDense();
    }

    std::vector<Texture>& GetTextures()
    {
        return m_textures.GetDense();
    }

private:
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

    SlotMap<Mesh> m_meshes;
    std::unordered_map<AssetID, MeshHandle> m_meshRegistry;

    std::unordered_map<MeshHandle, MeshBucket> m_buckets;

    std::unordered_map<MeshHandle, InstanceRange> m_instanceRanges;

    SlotMap<Material> m_materials;
    std::unordered_map<AssetID, MaterialHandle> m_materialRegistry;

    SlotMap<DirectionalLight> m_directionalLights;
    SlotMap<PointLight> m_pointLights;
    SlotMap<SpotLight> m_spotLights;

    SlotMap<Texture> m_textures;

    SlotMap<Entity> m_entities;
};