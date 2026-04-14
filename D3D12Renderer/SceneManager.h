#pragma once

#include <unordered_map>
#include <cassert>

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
        GetMesh(handle)->SetMaterial(GetMaterialHandle("builtin://default"));
        return handle;
    }

    Mesh* GetMesh(MeshHandle handle)
    {
        return m_meshes.Get(handle);
    }

    MeshHandle GetMeshHandle(const MeshName& name)
    {
        return m_meshRegistry[name];
    }

    void RegisterMesh(MeshHandle handle, const MeshName& name)
    {
        m_meshRegistry[name] = handle;
    }

    // Material
    MaterialHandle AddMaterial(ID3D12Device10* pDevice, DescriptorAllocation&& allocation)
    {
        return m_materials.Add(Material(pDevice, std::move(allocation)));
    }

    MaterialHandle AddMaterial(ID3D12Device10* pDevice, DescriptorAllocation&& allocation, const MaterialName& name)
    {
        auto handle = m_materials.Add(Material(pDevice, std::move(allocation)));
        m_materialRegistry[name] = handle;
        return handle;
    }

    Material* GetMaterial(MaterialHandle handle)
    {
        return m_materials.Get(handle);
    }

    MaterialHandle GetMaterialHandle(const MaterialName& name)
    {
        return m_materialRegistry[name];
    }

    void RegisterMaterial(MaterialHandle handle, const MaterialName& name)
    {
        m_materialRegistry[name] = handle;
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

    RenderObject* GetRenderObject(RenderObjectHandle handle)
    {
        return m_renderObjects.Get(handle);
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

    DirectionalLight* GetLight(DirectionalLightHandle handle)
    {
        return m_directionalLights.Get(handle);
    }

    PointLight* GetLight(PointLightHandle handle)
    {
        return m_pointLights.Get(handle);
    }

    SpotLight* GetLight(SpotLightHandle handle)
    {
        return m_spotLights.Get(handle);
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

    MaterialHandle FindMaterial(const MaterialName& name) const
    {
        auto it = m_materialRegistry.find(name);
        assert(it != m_materialRegistry.end());
        return it->second;
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
    SlotMap<Mesh> m_meshes;
    std::unordered_map<MeshName, MeshHandle> m_meshRegistry;
    // 모든 메쉬에 대한 순회는 dense array를 얻어서 간단하게 가능하지만,
    // CreateRenderObject같은 함수가 요구하는 '특정 메쉬`에 대한 정보는 SlotMap만으로는 부족함.
    // 즉 SlotMap은 handle -> data를 제공하지만, string(name) -> handle을 제공하는 구현체가 있어야 함.
    // 현재는 그것이 MeshRegistry, MaterialRegistry인데, ResourceManager에 통합하는게 더 깔끔할 것 같음. (아무래도 이름이 ResourceManager니까)

    SlotMap<RenderObject> m_renderObjects;
    std::unordered_map<MeshHandle, MeshBucket> m_buckets;

    std::unordered_map<MeshHandle, InstanceRange> m_instanceRanges;

    SlotMap<Material> m_materials;
    std::unordered_map<MaterialName, MaterialHandle> m_materialRegistry;

    SlotMap<DirectionalLight> m_directionalLights;
    SlotMap<PointLight> m_pointLights;
    SlotMap<SpotLight> m_spotLights;

    SlotMap<Texture> m_textures;
};