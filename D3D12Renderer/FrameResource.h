#pragma once

#include <Windows.h>
#include <wrl/client.h>

#include <d3d12.h>
#include <dxgi1_6.h>    // DXGI 1.6

#include <vector>
#include <memory>

#include "ConstantData.h"
#include "ConstantBuffer.h"
#include "DescriptorAllocation.h"

using Microsoft::WRL::ComPtr;

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
    FrameResource(ID3D12Device10* pDevice, IDXGISwapChain* pSwapChain, UINT frameIndex, DescriptorAllocation&& allocation);

//private:
    ComPtr<ID3D12Resource> m_renderTarget;
    DescriptorAllocation m_rtvAllocation;

    std::vector<std::unique_ptr<MeshCB>> m_meshConstantBuffers;
    std::vector<std::unique_ptr<MaterialCB>> m_materialConstantBuffers;
    std::vector<std::unique_ptr<LightCB>> m_lightConstantBuffers;
    std::vector<std::unique_ptr<CameraCB>> m_cameraConstantBuffers;
    std::unique_ptr<ShadowCB> m_shadowConstantBuffer;

    UINT64 m_fenceValue = 0;
};