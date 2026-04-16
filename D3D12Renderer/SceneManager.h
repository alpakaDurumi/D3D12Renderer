#pragma once

#include <unordered_map>
#include <cassert>
#include <variant>

#include "SlotMap.h"
#include "Mesh.h"
#include "Material.h"
#include "Light.h"
#include "RenderObject.h"
#include "Texture.h"
#include "Aliases.h"
#include "InstanceData.h"
#include "SceneHandles.h"

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

struct HierarchyNode
{
    HierarchyHandle selfHandle;
    //HierarchyHandle parent;
    std::variant<
        RenderObjectHandle,
        DirectionalLightHandle,
        PointLightHandle,
        SpotLightHandle> handle;
};

class SceneManager
{
public:
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

    RenderObjectHandle AddRenderObject(MeshHandle mesh)
    {
        return m_renderObjects.Add(RenderObject(mesh));
    }

    RenderObjectHandle AddRenderObject(MeshHandle mesh, const std::string& name)
    {
        auto handle = m_renderObjects.Add(RenderObject(mesh));
        m_renderObjects.Get(handle)->SetName(name);

        HierarchyNode node;
        node.handle = handle;
        auto hh = m_hierarchy.Add(std::move(node));
        m_hierarchy.Get(hh)->selfHandle = hh;

        return handle;
    }

    void Remove(HierarchyHandle hh)
    {
        if (!m_hierarchy.IsValid(hh)) return;

        auto* pEntry = m_hierarchy.Get(hh);

        std::visit([&](auto&& h) { Remove(h); }, pEntry->handle);

        m_hierarchy.Remove(hh);
    }

    const std::vector<RenderObject>& GetRenderObjects() const
    {
        return m_renderObjects.GetDense();
    }

    std::vector<RenderObject>& GetRenderObjects()
    {
        return m_renderObjects.GetDense();
    }

    std::vector<InstanceData> GatherInstances()
    {
        for (auto& [mesh, bucket] : m_buckets)
        {
            bucket.forward.clear();
            bucket.deferred.clear();
        }
        m_instanceRanges.clear();

        for (const auto& obj : m_renderObjects.GetDense())
        {
            auto meshHandle = obj.GetMesh();
            auto matHandle = obj.GetMaterial();

            // Use default material of Mesh if override material not set.
            if (!m_materials.IsValid(matHandle))
            {
                Mesh* pMesh = GetMesh(meshHandle);
                matHandle = pMesh->GetMaterial();
            }

            auto matIdx = m_materials.GetDenseIndex(matHandle);
            auto data = obj.BuildInstanceData(matIdx);

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

    RenderObject* Get(RenderObjectHandle h)
    {
        return m_renderObjects.Get(h);
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

    const std::vector<HierarchyNode>& GetHierarchy() const
    {
        return m_hierarchy.GetDense();
    }

    Object* Get(std::variant<
        RenderObjectHandle,
        DirectionalLightHandle,
        PointLightHandle,
        SpotLightHandle> handle)
    {
        return std::visit([this](auto&& h) -> Object*
            {
                return this->Get(h);
            }, handle);
    }

private:
    void Remove(RenderObjectHandle handle)
    {
        m_renderObjects.Remove(handle);
    }

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

    SlotMap<RenderObject> m_renderObjects;
    std::unordered_map<MeshHandle, MeshBucket> m_buckets;

    std::unordered_map<MeshHandle, InstanceRange> m_instanceRanges;

    SlotMap<Material> m_materials;
    std::unordered_map<AssetID, MaterialHandle> m_materialRegistry;

    SlotMap<DirectionalLight> m_directionalLights;
    SlotMap<PointLight> m_pointLights;
    SlotMap<SpotLight> m_spotLights;

    SlotMap<Texture> m_textures;

    SlotMap<HierarchyNode> m_hierarchy;
};