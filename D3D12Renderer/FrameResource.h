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

// Dynamic Data for each frame
class FrameResource
{
public:
    FrameResource(ID3D12Device10* pDevice, IDXGISwapChain* pSwapChain, UINT frameIndex, DescriptorAllocation&& allocation)
        : m_rtvAllocation(std::move(allocation))
    {
        ThrowIfFailed(pSwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_renderTarget)));
        pDevice->CreateRenderTargetView(m_renderTarget.Get(), nullptr, m_rtvAllocation.GetDescriptorHandle());
    }
    
    ~FrameResource()
    {
        for (auto* pMeshCB : m_meshConstantBuffers)
            delete pMeshCB;
        for (auto* pMatCB : m_materialConstantBuffers)
            delete pMatCB;
        delete m_lightConstantBuffer;
        delete m_cameraConstantBuffer;
    }

//private:
    ComPtr<ID3D12Resource> m_renderTarget;
    DescriptorAllocation m_rtvAllocation;

    std::vector<MeshCB*> m_meshConstantBuffers;
    std::vector<MaterialCB*> m_materialConstantBuffers;
    LightCB* m_lightConstantBuffer = nullptr;
    CameraCB* m_cameraConstantBuffer = nullptr;

    UINT64 m_fenceValue = 0;
};