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
    FrameResource(ComPtr<ID3D12Device10>& device, ComPtr<IDXGISwapChain3>& swapChain, UINT frameIndex, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
    {
        ThrowIfFailed(swapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_renderTarget)));
        device->CreateRenderTargetView(m_renderTarget.Get(), nullptr, rtvHandle);
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

    std::vector<MeshCB*> m_meshConstantBuffers;
    std::vector<MaterialCB*> m_materialConstantBuffers;
    // �Ʒ� CB�鵵 ������ �þ�� �迭�� �����ϰ� �� �� ����
    LightCB* m_lightConstantBuffer = nullptr;
    CameraCB* m_cameraConstantBuffer = nullptr;

    UINT64 m_fenceValue = 0;
};