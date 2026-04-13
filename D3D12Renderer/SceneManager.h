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
    MeshHandle AddMesh(Mesh&& mesh)
    {
        return m_meshes.Add(std::move(mesh));
    }

    Mesh* GetMesh(MeshHandle handle)
    {
        return m_meshes.Get(handle);
    }

    void RegisterMesh(MeshHandle handle, MeshName& name)
    {
        m_meshRegistry[name] = handle;
    }

    // Material
    MaterialHandle AddMaterial(Material&& material)
    {
        return m_materials.Add(std::move(material));
    }

    Material* GetMaterial(MaterialHandle handle)
    {
        return m_materials.Get(handle);
    }

    void RegisterMaterial(MaterialHandle handle, MaterialName& name)
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

    const std::vector<RenderObject>& GetRenderObjects() const
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

    //RenderObject* GetRenderObject(RenderObjectHandle handle)
    //{
    //    auto& groups = handle.path == RenderingPath::FORWARD ? m_forwardRenderGroups : m_deferredRenderGroups;
    //    return groups[handle.meshHandle].Get(handle.slotHandle);
    //}

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

    MaterialHandle FindMaterial(MaterialName name) const
    {
        auto it = m_materialRegistry.find(name);
        assert(it != m_materialRegistry.end());
        return it->second;
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