#pragma once

#include <unordered_map>
#include <variant>
#include <cassert>
#include <queue>
#include <functional>

#include "SlotMap.h"
#include "Mesh.h"
#include "Material.h"
#include "Light.h"
#include "Aliases.h"
#include "InstanceData.h"
#include "SceneHandles.h"
#include "Transform.h"
#include "Texture.h"
#include "View.h"
#include "Utility.h"
#include "DDSTextureLoader12.h"

template<>
struct std::hash<MeshHandle>
{
    std::size_t operator()(const MeshHandle& h) const
    {
        return (static_cast<UINT64>(h.index) << 32) | static_cast<UINT64>(h.generation);
    }
};

template<>
struct std::hash<EntityHandle>
{
    std::size_t operator()(const EntityHandle& h) const
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

struct AssetTexture
{
    Texture texture;
    ShaderResourceView srv;
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
        {
            auto lightHandle = pEntity->light.value();

            auto resources = std::visit([&](auto&& handle) { return Get(handle)->TakeResources(); }, lightHandle);
            for (auto& res : resources)
                m_deferred.push_back(std::move(res));

            // It is valid to Remove Light after moving resources
            std::visit([&](auto&& handle) { Remove(handle); }, lightHandle);
        }

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
        m_entityIndexInBucket.clear();      // 실제로 entity dense array에 변화가 있을 때만 clear하거나, 구조를 개선하기.

        for (const auto& entity : m_entities.GetDense())
        {
            if (!entity.meshRenderer.has_value()) continue;

            auto meshHandle = entity.meshRenderer->mesh;
            auto matHandle = entity.meshRenderer->material;

            auto matIdx = m_materials.GetDenseIndex(matHandle);
            auto data = BuildInstanceData(entity.transform->GetWorldRenderTransform(), matIdx);

            auto renderingPath = GetMaterial(matHandle)->GetRenderingPath();

            if (renderingPath == RenderingPath::FORWARD)
            {
                m_entityIndexInBucket[entity.selfHandle] = static_cast<UINT>(m_buckets[meshHandle].forward.size());
                m_buckets[meshHandle].forward.push_back(data);
            }
            else
            {
                m_entityIndexInBucket[entity.selfHandle] = static_cast<UINT>(m_buckets[meshHandle].deferred.size());
                m_buckets[meshHandle].deferred.push_back(data);
            }
        }

        for (const auto& entity : m_entities.GetDense())
        {
            if (!entity.meshRenderer.has_value()) continue;

            auto meshHandle = entity.meshRenderer->mesh;
            auto matHandle = entity.meshRenderer->material;

            auto renderingPath = GetMaterial(matHandle)->GetRenderingPath();

            if (renderingPath == RenderingPath::DEFERRED)
                m_entityIndexInBucket[entity.selfHandle] += static_cast<UINT>(m_buckets[meshHandle].forward.size());
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

    UINT GetEntityIndexInBucket(EntityHandle entity)
    {
        return m_entityIndexInBucket[entity];
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

    AssetTextureHandle AddAssetTexture(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& srvAllocation,
        TransientUploadAllocator& uploadAllocator,
        const std::vector<UINT8>& textureSrc,
        UINT width,
        UINT height)
    {
        auto resourceDesc = GetTexture2DDesc(width, height, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
        Texture texture(pDevice, resourceDesc, D3D12_BARRIER_LAYOUT_COPY_DEST, nullptr, D3D12_HEAP_TYPE_DEFAULT);

        // Calculate required size for data upload
        D3D12_RESOURCE_DESC desc = texture.Get()->GetDesc();
        UINT64 requiredSize = 0;
        pDevice->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);

        auto uploadAllocation = uploadAllocator.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = textureSrc.data();
        textureData.RowPitch = width * 4;   // 4 bytes per pixel (RGBA)
        textureData.SlicePitch = textureData.RowPitch * height;

        D3DHelper::UpdateSubresources(pDevice, pCommandList, texture.Get(), uploadAllocation.pResource, uploadAllocation.Offset, uploadAllocation.CPUPtr, 0, 1, &textureData);

        D3D12_TEXTURE_BARRIER barrier1 = {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
            texture.Get(),
            {0xffff'ffff, 0, 0, 0, 0, 0},
            D3D12_TEXTURE_BARRIER_FLAG_NONE
        };

        D3D12_BARRIER_GROUP barrierGroups1[] = { D3DHelper::TextureBarrierGroup(1, &barrier1) };
        pCommandList->Barrier(1, barrierGroups1);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        ShaderResourceView srv(pDevice, texture.Get(), srvDesc, std::move(srvAllocation));

        return m_assetTextures.Add(AssetTexture{ std::move(texture), std::move(srv) });
    }

    AssetTextureHandle AddAssetTexture(
        ID3D12Device10* pDevice,
        ID3D12GraphicsCommandList7* pCommandList,
        DescriptorAllocation&& srvAllocation,
        TransientUploadAllocator& uploadAllocator,
        const std::wstring& filePath,
        bool isSRGB,
        bool useBlockCompress,
        bool flipImage,
        bool isCubeMap)
    {
        // Find file and check validity
        std::wstring ddsFilePath = Utility::RemoveFileExtension(filePath) + L".dds";

        struct _stat64 ddsFileStat, srcFileStat;

        bool srcFileMissing = _wstat64(filePath.c_str(), &srcFileStat) == -1;
        bool ddsFileMissing = _wstat64(ddsFilePath.c_str(), &ddsFileStat) == -1;

        if (srcFileMissing)
        {
            throw std::runtime_error("File not found.");
        }

        // If dds file does not exist or older than src file
        if (ddsFileMissing || ddsFileStat.st_mtime < srcFileStat.st_mtime)
        {
            D3DHelper::ConvertToDDS(filePath, isSRGB, useBlockCompress, flipImage);
        }

        ComPtr<ID3D12Resource> resource;
        std::unique_ptr<uint8_t[]> ddsData;
        std::vector<D3D12_SUBRESOURCE_DATA> subresources;

        // LoadDDSTextureFromFile creates a resource with an initial state of D3D12_RESOURCE_STATE_COMMON
        // It corresponds to D3D12_BARRIER_LAYOUT_COMMON in Enhanced Barriers context
        D3DHelper::ThrowIfFailed(DirectX::LoadDDSTextureFromFile(
            pDevice,
            ddsFilePath.c_str(),
            &resource,
            ddsData,
            subresources));

        Texture texture(std::move(resource));

        D3D12_RESOURCE_DESC desc = texture.Get()->GetDesc();
        UINT numSubresources = static_cast<UINT>(subresources.size());

        D3D12_TEXTURE_BARRIER barrier0 = {
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_LAYOUT_COMMON,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            texture.Get(),
            {0xffff'ffff, 0, 0, 0, 0, 0},
            D3D12_TEXTURE_BARRIER_FLAG_NONE
        };

        D3D12_BARRIER_GROUP barrierGroups0[] = { D3DHelper::TextureBarrierGroup(1, &barrier0) };
        pCommandList->Barrier(1, barrierGroups0);

        // Calculate required size for data upload
        UINT64 requiredSize = 0;
        pDevice->GetCopyableFootprints(&desc, 0, numSubresources, 0, nullptr, nullptr, nullptr, &requiredSize);

        auto uploadAllocation = uploadAllocator.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        D3DHelper::UpdateSubresources(pDevice, pCommandList, texture.Get(), uploadAllocation.pResource, uploadAllocation.Offset, uploadAllocation.CPUPtr, 0, numSubresources, subresources.data());

        D3D12_TEXTURE_BARRIER barrier1 = {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
            texture.Get(),
            {0xffff'ffff, 0, 0, 0, 0, 0},
            D3D12_TEXTURE_BARRIER_FLAG_NONE
        };

        D3D12_BARRIER_GROUP barrierGroups1[] = { D3DHelper::TextureBarrierGroup(1, &barrier1) };
        pCommandList->Barrier(1, barrierGroups1);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D)
        {
            if (desc.DepthOrArraySize == 1)
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
                srvDesc.Texture1D.MipLevels = desc.MipLevels;
            }
            else
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
                srvDesc.Texture1DArray.MipLevels = desc.MipLevels;
            }
        }
        else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
        {
            if (desc.DepthOrArraySize == 1)
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = desc.MipLevels;
            }
            else if (desc.DepthOrArraySize % 6 == 0 && isCubeMap)
            {
                if (desc.DepthOrArraySize / 6 == 1)
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                    srvDesc.TextureCube.MipLevels = desc.MipLevels;
                }
                else
                {
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                    srvDesc.TextureCubeArray.MipLevels = desc.MipLevels;
                }
            }
            else
            {
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
            }
        }
        else    // TEXTURE3D
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MipLevels = desc.MipLevels;
        }

        ShaderResourceView srv(pDevice, texture.Get(), srvDesc, std::move(srvAllocation));

        return m_assetTextures.Add(AssetTexture{ std::move(texture), std::move(srv) });
    }

    const std::vector<AssetTexture>& GetAssetTextures() const
    {
        return m_assetTextures.GetDense();
    }

    std::vector<AssetTexture>& GetAssetTextures()
    {
        return m_assetTextures.GetDense();
    }

    void QueueDeferredDeletions(UINT64 fenceValue, UINT64 completedFenceValue)
    {
        // 1. push resources to queue with fenceValue
        for (auto& res : m_deferred)
            m_deletionQueue.emplace(fenceValue, std::move(res));
        m_deferred.clear();

        // 2. Delete resources that completed in GPU timeline
        while (!m_deletionQueue.empty() && m_deletionQueue.front().fenceValue <= completedFenceValue)
            m_deletionQueue.pop();
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

    std::unordered_map<EntityHandle, UINT> m_entityIndexInBucket;

    SlotMap<Material> m_materials;
    std::unordered_map<AssetID, MaterialHandle> m_materialRegistry;

    SlotMap<DirectionalLight> m_directionalLights;
    SlotMap<PointLight> m_pointLights;
    SlotMap<SpotLight> m_spotLights;

    SlotMap<AssetTexture> m_assetTextures;

    SlotMap<Entity> m_entities;

    std::vector<GpuResource> m_deferred;     // List of resources requested to be removed

    struct DeferredResource
    {
        DeferredResource(UINT64 fenceValue, GpuResource&& resource)
            : fenceValue(fenceValue), resource(std::move(resource))
        {
        }

        UINT64 fenceValue;
        GpuResource resource;
    };
    std::queue<DeferredResource> m_deletionQueue;
};