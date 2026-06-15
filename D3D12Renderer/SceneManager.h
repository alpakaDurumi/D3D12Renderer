#pragma once

#include <cassert>
#include <iterator>
#include <memory>
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
#include <wrl/client.h>

#include <DDSTextureLoader12.h>

#include "Aliases.h"
#include "GeometryData.h"
#include "InstanceData.h"
#include "Light.h"
#include "Material.h"
#include "Mesh.h"
#include "SceneHandles.h"
#include "SlotMap.h"
#include "Texture.h"
#include "Transform.h"
#include "TransientUploadAllocator.h"
#include "Utility.h"
#include "View.h"

struct InstanceRange
{
    UINT baseIndex;
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
        if (!m_entities.IsValid(handle))
            return;

        auto* pEntity = m_entities.Get(handle);

        // Should delete material with 0 usage?

        if (pEntity->light.has_value())
        {
            auto lightHandle = pEntity->light.value();
            std::visit(
                [&](auto&& handle)
                {
                    auto resources = Get(handle)->TakeResources();
                    m_deferred.insert(
                        m_deferred.end(),
                        std::make_move_iterator(resources.begin()),
                        std::make_move_iterator(resources.end()));
                    Remove(handle);
                },
                lightHandle);
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
        if (pEntity->transform.has_value())
            assert(false);

        pEntity->transform.emplace();
    }

    void AddTransform(EntityHandle eh, const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& eulerRad, const DirectX::XMFLOAT3& t)
    {
        auto* pEntity = m_entities.Get(eh);
        if (pEntity->transform.has_value())
            assert(false);

        pEntity->transform.emplace(s, eulerRad, t);
    }

    void ApplyTransform(EntityHandle eh, const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& eulerRad, const DirectX::XMFLOAT3& t)
    {
        auto* pEntity = m_entities.Get(eh);
        if (!pEntity->transform.has_value())
            assert(false);
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
            pEntity->meshRenderer = {mh, GetMaterialHandle("builtin://material/default")};
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

    InstanceData BuildInstanceData(const DirectX::XMFLOAT4X4& transform, UINT matIdx) const
    {
        InstanceData ret;

        auto world = DirectX::XMLoadFloat4x4(&transform);

        DirectX::XMStoreFloat4x4(&ret.world, DirectX::XMMatrixTranspose(world));

        world.r[3] = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        DirectX::XMStoreFloat4x4(&ret.inverseTranspose, DirectX::XMMatrixInverse(nullptr, world));

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
        m_entityIndex.clear();

        std::unordered_map<MeshHandle, std::pair<std::vector<EntityHandle>, std::vector<EntityHandle>>> temp;

        for (const auto& entity : m_entities.GetDense())
        {
            if (!entity.meshRenderer.has_value())
                continue;

            auto meshHandle = entity.meshRenderer->mesh;
            auto matHandle = entity.meshRenderer->material;

            auto matIdx = m_materials.GetDenseIndex(matHandle);
            auto data = BuildInstanceData(entity.transform->GetWorldRenderTransform(), matIdx);

            auto renderingPath = GetMaterial(matHandle)->GetRenderingPath();

            if (renderingPath == RenderingPath::FORWARD)
            {
                m_buckets[meshHandle].forward.push_back(data);
                temp[meshHandle].first.push_back(entity.selfHandle);
            }
            else
            {
                m_buckets[meshHandle].deferred.push_back(data);
                temp[meshHandle].second.push_back(entity.selfHandle);
            }
        }

        UINT currentIndex = 0;

        std::vector<InstanceData> ret;
        m_worldBoundingSpheres.resize(m_entities.GetCount());

        for (const auto& [meshHandle, bucket] : m_buckets)
        {
            const auto& [forward, deferred] = bucket;

            InstanceRange& range = m_instanceRanges[meshHandle];
            range.baseIndex = currentIndex;
            range.forwardCount = static_cast<UINT>(forward.size());
            range.deferredCount = static_cast<UINT>(deferred.size());

            ret.insert(ret.end(), forward.begin(), forward.end());
            ret.insert(ret.end(), deferred.begin(), deferred.end());

            currentIndex += range.forwardCount + range.deferredCount;

            // Compute per-instance world bounding sphere
            const DirectX::BoundingSphere& local = GetMesh(meshHandle)->GetBoundingSphere();

            const UINT base = range.baseIndex;

            const auto& owners = temp[meshHandle]; // EntityHandles that use meshHandle

            for (UINT k = 0; k < range.forwardCount; ++k)
            {
                const UINT index = base + k;
                DirectX::XMMATRIX world = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&forward[k].world));
                local.Transform(m_worldBoundingSpheres[index], world);
                m_entityIndex[owners.first[k]] = index;
            }

            for (UINT k = 0; k < range.deferredCount; ++k)
            {
                const UINT index = base + range.forwardCount + k;
                DirectX::XMMATRIX world = DirectX::XMMatrixTranspose(DirectX::XMLoadFloat4x4(&deferred[k].world));
                local.Transform(m_worldBoundingSpheres[index], world);
                m_entityIndex[owners.second[k]] = index;
            }
        }

        return ret;
    }

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
        auto resourceDesc = D3DHelper::GetTexture2DDesc(width, height, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
        Texture texture(pDevice, resourceDesc, D3D12_BARRIER_LAYOUT_COPY_DEST, nullptr, D3D12_HEAP_TYPE_DEFAULT);

        // Calculate required size for data upload
        D3D12_RESOURCE_DESC desc = texture.Get()->GetDesc();
        UINT64 requiredSize = 0;
        pDevice->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);

        auto uploadAllocation = uploadAllocator.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = textureSrc.data();
        textureData.RowPitch = width * 4; // 4 bytes per pixel (RGBA)
        textureData.SlicePitch = textureData.RowPitch * height;

        D3DHelper::UpdateSubresources(pDevice, pCommandList, texture.Get(), uploadAllocation.pResource, uploadAllocation.offset, uploadAllocation.cpuPtr, 0, 1, &textureData);

        D3D12_TEXTURE_BARRIER barrier1 = {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
            texture.Get(),
            {0xffff'ffff, 0, 0, 0, 0, 0},
            D3D12_TEXTURE_BARRIER_FLAG_NONE};

        D3D12_BARRIER_GROUP barrierGroups1[] = {D3DHelper::TextureBarrierGroup(1, &barrier1)};
        pCommandList->Barrier(1, barrierGroups1);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        ShaderResourceView srv(pDevice, texture.Get(), srvDesc, std::move(srvAllocation));

        return m_assetTextures.Add(AssetTexture{std::move(texture), std::move(srv)});
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

        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        std::unique_ptr<UINT8[]> ddsData;
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
            D3D12_TEXTURE_BARRIER_FLAG_NONE};

        D3D12_BARRIER_GROUP barrierGroups0[] = {D3DHelper::TextureBarrierGroup(1, &barrier0)};
        pCommandList->Barrier(1, barrierGroups0);

        // Calculate required size for data upload
        UINT64 requiredSize = 0;
        pDevice->GetCopyableFootprints(&desc, 0, numSubresources, 0, nullptr, nullptr, nullptr, &requiredSize);

        auto uploadAllocation = uploadAllocator.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

        D3DHelper::UpdateSubresources(pDevice, pCommandList, texture.Get(), uploadAllocation.pResource, uploadAllocation.offset, uploadAllocation.cpuPtr, 0, numSubresources, subresources.data());

        D3D12_TEXTURE_BARRIER barrier1 = {
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_COPY_DEST,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
            texture.Get(),
            {0xffff'ffff, 0, 0, 0, 0, 0},
            D3D12_TEXTURE_BARRIER_FLAG_NONE};

        D3D12_BARRIER_GROUP barrierGroups1[] = {D3DHelper::TextureBarrierGroup(1, &barrier1)};
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
        else // TEXTURE3D
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MipLevels = desc.MipLevels;
        }

        ShaderResourceView srv(pDevice, texture.Get(), srvDesc, std::move(srvAllocation));

        return m_assetTextures.Add(AssetTexture{std::move(texture), std::move(srv)});
    }

    const std::vector<AssetTexture>& GetAssetTextures() const
    {
        return m_assetTextures.GetDense();
    }

    std::vector<AssetTexture>& GetAssetTextures()
    {
        return m_assetTextures.GetDense();
    }

    // push resources to queue with signaledFenceValue
    void QueueDeferredDeletions(UINT64 signaledFenceValue)
    {
        for (auto& res : m_deferred)
            m_deletionQueue.emplace(signaledFenceValue, std::move(res));
        m_deferred.clear();
    }

    // Delete resources that completed in GPU timeline
    void ProcessCompletedDeletions(UINT64 completedFenceValue)
    {
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
    std::queue<DeferredResource> m_deletionQueue;
};
