#pragma once

#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6
#include <DirectXMath.h>

#include "D3DHelper.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace D3DHelper;

// Dynamic Data for each frame
class FrameResource
{
public:
    FrameResource(ComPtr<ID3D12Device> device, ComPtr<IDXGISwapChain3> swapChain, UINT frameIndex, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle)
    {
        ThrowIfFailed(swapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_renderTarget)));
        device->CreateRenderTargetView(m_renderTarget.Get(), nullptr, rtvHandle);

        ThrowIfFailed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
    }

//private:
    ComPtr<ID3D12Resource> m_renderTarget;
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;

    // 전역적으로 쓰이는 constant buffer와 그렇지 않은 constant buffer를 따로 관리해야 할 것 같다.
    ComPtr<ID3D12Resource> m_sceneConstantBuffer;
    UINT8* m_pSceneBufferBegin;

    ComPtr<ID3D12Resource> m_materialConstantBuffer;
    UINT8* m_pMaterialBufferBegin;

    ComPtr<ID3D12Resource> m_lightConstantBuffer;
    UINT8* m_pLightBufferBegin;

    ComPtr<ID3D12Resource> m_cameraConstantBuffer;
    UINT8* m_pCameraBufferBegin;

    UINT64 m_fenceValue = 0;
};