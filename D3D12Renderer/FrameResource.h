#pragma once

#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6
#include <DirectXMath.h>

#include <vector>

#include "D3DHelper.h"
#include "ConstantData.h"
#include "ConstantBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
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
    FrameResource(ComPtr<ID3D12Device>& device, ComPtr<IDXGISwapChain3>& swapChain, UINT frameIndex, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
    {
        ThrowIfFailed(swapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_renderTarget)));
        device->CreateRenderTargetView(m_renderTarget.Get(), nullptr, rtvHandle);

        ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
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
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;

    std::vector<MeshCB*> m_meshConstantBuffers;
    std::vector<MaterialCB*> m_materialConstantBuffers;
    // 아래 CB들도 개수가 늘어나면 배열로 관리하게 될 수 있음
    LightCB* m_lightConstantBuffer;
    CameraCB* m_cameraConstantBuffer;

    UINT64 m_fenceValue = 0;
};