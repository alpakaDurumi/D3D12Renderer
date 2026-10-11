#include "pch.h"

#include "SceneManager.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <stdexcept>

#include <DirectXMath.h>
#include <sys/stat.h>
#include <wrl/client.h>

#include <DDSTextureLoader12.h>

#include "D3DHelper.h"
#include "DescriptorAllocation.h"
#include "GeometryData.h"
#include "TransientUploadAllocator.h"
#include "Utility.h"

using namespace DirectX;

static InstanceData BuildInstanceData(const XMFLOAT4X4& transform, UINT matIdx)
{
    InstanceData ret;

    auto world = XMLoadFloat4x4(&transform);

    XMStoreFloat4x4(&ret.world, XMMatrixTranspose(world));

    world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    XMStoreFloat4x4(&ret.inverseTranspose, XMMatrixInverse(nullptr, world));

    ret.materialIndex = matIdx;

    return ret;
}

EntityHandle SceneManager::AddEntity(const std::string& name)
{
    auto handle = m_entities.Add(Entity());

    auto* pEntity = m_entities.Get(handle);
    pEntity->name = name;
    pEntity->selfHandle = handle;

    return handle;
}

void SceneManager::Remove(EntityHandle handle)
{
    if (!m_entities.IsValid(handle))
        return;

    auto* pEntity = m_entities.Get(handle);

    // Should delete material with 0 usage?

    if (pEntity->light.has_value())
    {
        auto lightHandle = pEntity->light.value();
        std::visit(
            [&](auto&& handle) {
                auto resources = Get(handle)->TakeResources();
                EnqueueResourceDeletion(resources);
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

void SceneManager::AddChild(EntityHandle parent, EntityHandle child)
{
    auto* pParent = m_entities.Get(parent);
    auto* pChild = m_entities.Get(child);

    pParent->children.push_back(child);
    pChild->parent = parent;
}

void SceneManager::SetMesh(EntityHandle eh, MeshHandle mh)
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

void SceneManager::SetMaterial(EntityHandle eh, MaterialHandle mh)
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

MeshHandle SceneManager::AddMesh(
    ID3D12GraphicsCommandList7* pCommandList,
    TransientUploadAllocator& allocator,
    const GeometryData& data)
{
    auto handle = m_meshes.Add(Mesh(m_pDevice, pCommandList, allocator, data));
    m_meshRegistry[data.name] = handle;
    GetMesh(handle)->SetMaterial(GetMaterialHandle("builtin://material/default"));
    return handle;
}

UINT SceneManager::GetShadowMapResolution(LightHandle handle)
{
    UINT resolution = 0;
    std::visit(
        [&](auto&& lightHandle) { resolution = Get(lightHandle)->GetShadowMapResolution(); },
        handle);

    return resolution;
}

void SceneManager::SetShadowMapResolution(LightHandle handle, UINT resolution)
{
    std::visit(
        [&](auto&& lightHandle) {
            auto* pLight = Get(lightHandle);
            if (pLight->GetShadowMapResolution() == resolution) return;

            auto resources = pLight->TakeResources();
            EnqueueResourceDeletion(resources);
            pLight->ChangeShadowMapResolution(m_pDevice, resolution);
        },
        handle);
}

AssetTextureHandle SceneManager::AddAssetTexture(
    ID3D12GraphicsCommandList7* pCommandList,
    DescriptorAllocation&& srvAllocation,
    TransientUploadAllocator& uploadAllocator,
    const std::vector<UINT8>& textureSrc,
    UINT width,
    UINT height)
{
    auto resourceDesc = D3DHelper::GetTexture2DDesc(width, height, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
    Texture texture(m_pDevice, resourceDesc, D3D12_BARRIER_LAYOUT_COPY_DEST, nullptr, D3D12_HEAP_TYPE_DEFAULT);

    // Calculate required size for data upload
    D3D12_RESOURCE_DESC desc = texture.Get()->GetDesc();
    UINT64 requiredSize = 0;
    m_pDevice->GetCopyableFootprints(&desc, 0, 1, 0, nullptr, nullptr, nullptr, &requiredSize);

    auto uploadAllocation = uploadAllocator.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = textureSrc.data();
    textureData.RowPitch = width * 4; // 4 bytes per pixel (RGBA)
    textureData.SlicePitch = textureData.RowPitch * height;

    D3DHelper::UpdateSubresources(m_pDevice, pCommandList, texture.Get(), uploadAllocation.pResource, uploadAllocation.offset, uploadAllocation.cpuPtr, 0, 1, &textureData);

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

    ShaderResourceView srv(m_pDevice, texture.Get(), srvDesc, std::move(srvAllocation));

    return m_assetTextures.Add(AssetTexture{std::move(texture), std::move(srv)});
}

AssetTextureHandle SceneManager::AddAssetTexture(
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
    D3DHelper::ThrowIfFailed(LoadDDSTextureFromFile(
        m_pDevice,
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
    m_pDevice->GetCopyableFootprints(&desc, 0, numSubresources, 0, nullptr, nullptr, nullptr, &requiredSize);

    auto uploadAllocation = uploadAllocator.Allocate(requiredSize, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    D3DHelper::UpdateSubresources(m_pDevice, pCommandList, texture.Get(), uploadAllocation.pResource, uploadAllocation.offset, uploadAllocation.cpuPtr, 0, numSubresources, subresources.data());

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

    ShaderResourceView srv(m_pDevice, texture.Get(), srvDesc, std::move(srvAllocation));

    return m_assetTextures.Add(AssetTexture{std::move(texture), std::move(srv)});
}

void SceneManager::UpdateWorldTransforms(float alpha)
{
    for (auto& entity : m_entities.GetDense())
    {
        if (entity.parent.Empty())
        {
            XMMATRIX accumulated = XMMatrixIdentity();
            UpdateWorldTransform(entity, accumulated, alpha);
        }
    }
}

std::vector<InstanceData> SceneManager::GatherInstances()
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
        auto data = BuildInstanceData(entity.transform.GetWorldRenderTransform(), matIdx);

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
        const BoundingSphere& local = GetMesh(meshHandle)->GetBoundingSphere();

        const UINT base = range.baseIndex;

        const auto& owners = temp[meshHandle]; // EntityHandles that use meshHandle

        for (UINT k = 0; k < range.forwardCount; ++k)
        {
            const UINT index = base + k;
            XMMATRIX world = XMMatrixTranspose(XMLoadFloat4x4(&forward[k].world));
            local.Transform(m_worldBoundingSpheres[index], world);
            m_entityIndex[owners.first[k]] = index;
        }

        for (UINT k = 0; k < range.deferredCount; ++k)
        {
            const UINT index = base + range.forwardCount + k;
            XMMATRIX world = XMMatrixTranspose(XMLoadFloat4x4(&deferred[k].world));
            local.Transform(m_worldBoundingSpheres[index], world);
            m_entityIndex[owners.second[k]] = index;
        }
    }

    return ret;
}

void SceneManager::EnqueueResourceDeletion(std::vector<GpuResource>& resources)
{
    m_deferred.insert(
        m_deferred.end(),
        std::make_move_iterator(resources.begin()),
        std::make_move_iterator(resources.end()));
}

// push resources to queue with signaledFenceValue
void SceneManager::QueueDeferredDeletions(UINT64 signaledFenceValue)
{
    for (auto& res : m_deferred)
        m_deletionQueue.emplace(signaledFenceValue, std::move(res));
    m_deferred.clear();
}

// Delete resources that completed in GPU timeline
void SceneManager::ProcessCompletedDeletions(UINT64 completedFenceValue)
{
    while (!m_deletionQueue.empty() && m_deletionQueue.front().fenceValue <= completedFenceValue)
        m_deletionQueue.pop();
}

void XM_CALLCONV SceneManager::UpdateWorldTransform(Entity& entity, FXMMATRIX accumulated, float alpha)
{
    entity.transform.UpdateLocalRenderState(alpha);
    XMMATRIX localRenderTransform = XMLoadFloat4x4(&entity.transform.GetLocalRenderTransform());

    XMMATRIX world = localRenderTransform * accumulated;
    entity.transform.SetWorldRenderTransform(world);

    // If Light component exists, set position and direction
    if (entity.light.has_value())
    {
        auto lightHandle = entity.light.value();

        std::visit(
            [&](auto&& handle) {
                Get(handle)->SetWorldTransform(world);
            },
            lightHandle);
    }

    for (auto& child : entity.children)
        UpdateWorldTransform(*Get(child), world, alpha);
}
