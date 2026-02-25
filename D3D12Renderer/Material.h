#pragma once

#include <d3d12.h>

#include <vector>
#include <memory>
#include <utility>

#include "ConstantData.h"
#include "DescriptorAllocation.h"
#include "FrameResource.h"

class Material
{
public:
    Material(
        ID3D12Device10* pDevice,
        DescriptorAllocation&& allocation,
        const std::vector<std::unique_ptr<FrameResource>>& frameResources)
    {
        assert(allocation.GetNumHandles() == frameResources.size());

        auto allocations = allocation.Split();

        // Create constant buffers
        for (UINT i = 0; i < frameResources.size(); ++i)
        {
            FrameResource& frameResource = *frameResources[i];

            if (i == 0) m_constantBufferIndex = UINT(frameResource.m_materialConstantBuffers.size());
            frameResource.m_materialConstantBuffers.push_back(std::make_unique<MaterialCB>(pDevice, std::move(allocations[i])));
        }
    }

    UINT GetMaterialConstantBufferIndex() const
    {
        return m_constantBufferIndex;
    }

    void SetAmbient(XMFLOAT4 ambient)
    {
        m_constantData.SetAmbient(ambient);
    }

    void SetSpecular(XMFLOAT4 specular)
    {
        m_constantData.SetSpecular(specular);
    }

    void SetShininess(float shininess)
    {
        m_constantData.shininess = shininess;
    }

    void SetTextureIndices(UINT albedoIdx, UINT normalMapIdx, UINT heightMapIdx)
    {
        m_constantData.textureIndices[0] = albedoIdx;
        m_constantData.textureIndices[1] = normalMapIdx;
        m_constantData.textureIndices[2] = heightMapIdx;
    }

    void UpdateMaterialConstantBuffer(FrameResource& frameResource)
    {
        frameResource.m_materialConstantBuffers[m_constantBufferIndex]->Update(&m_constantData);
    }

private:
    MaterialConstantData m_constantData;
    UINT m_constantBufferIndex;
};