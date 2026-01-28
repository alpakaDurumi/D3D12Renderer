#include "pch.h"
#include "FrameResource.h"

#include "D3DHelper.h"

FrameResource::FrameResource(ID3D12Device10* pDevice, IDXGISwapChain* pSwapChain, UINT frameIndex, DescriptorAllocation&& allocation)
    : m_rtvAllocation(std::move(allocation))
{
    assert(!m_rtvAllocation.IsNull());

    D3DHelper::ThrowIfFailed(pSwapChain->GetBuffer(frameIndex, IID_PPV_ARGS(&m_renderTarget)));

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    pDevice->CreateRenderTargetView(m_renderTarget.Get(), &rtvDesc, m_rtvAllocation.GetDescriptorHandle());
}