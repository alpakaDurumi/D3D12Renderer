#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6

#include <vector>
#include <utility>

#include "D3DHelper.h"
#include "ConstantData.h"
#include "ConstantBuffer.h"
#include "DescriptorAllocation.h"

using Microsoft::WRL::ComPtr;
using namespace D3DHelper;

// aliasing
using MeshCB = ConstantBuffer<MeshConstantData>;
using MaterialCB = ConstantBuffer<MaterialConstantData>;
using LightCB = ConstantBuffer<LightConstantData>;
using CameraCB = ConstantBuffer<CameraConstantData>;
using ShadowCB = ConstantBuffer<ShadowConstantData>;

// Dynamic Data for each frame
class FrameResource
{
public:
    FrameResource(ID3D12Device10* pDevice, IDXGISwapChain* pSwapChain, UINT frameIndex, DescriptorAllocation&& allocation)
        : m_rtvAllocation(std::move(allocation))
    {
        ThrowIfFailed(pSwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_renderTarget)));

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        pDevice->CreateRenderTargetView(m_renderTarget.Get(), &rtvDesc, m_rtvAllocation.GetDescriptorHandle());
    }
    
    ~FrameResource()
    {
        for (auto* pMeshCB : m_meshConstantBuffers)
            delete pMeshCB;
        for (auto* pMatCB : m_materialConstantBuffers)
            delete pMatCB;
        for (auto* pLightCB : m_lightConstantBuffers)
            delete pLightCB;
        for (auto* pCameraCB : m_cameraConstantBuffers)
            delete pCameraCB;
        delete m_shadowConstantBuffer;
    }

//private:
    ComPtr<ID3D12Resource> m_renderTarget;
    DescriptorAllocation m_rtvAllocation;

    std::vector<MeshCB*> m_meshConstantBuffers;
    std::vector<MaterialCB*> m_materialConstantBuffers;
    std::vector<LightCB*> m_lightConstantBuffers;
    std::vector<CameraCB*> m_cameraConstantBuffers;
    ShadowCB* m_shadowConstantBuffer;

    UINT64 m_fenceValue = 0;
};