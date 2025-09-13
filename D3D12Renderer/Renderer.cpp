#include "Renderer.h"
#include "Win32Application.h"
#include "D3DHelper.h"
#include <D3Dcompiler.h>

// 이거 일단 빼야겠다. nuget에서도 제외시키자
//#include <dxcapi.h>
//#include <d3d12shader.h>

using namespace D3DHelper;

// Generate a simple black and white checkerboard texture.
std::vector<UINT8> GenerateTextureData(UINT textureWidth, UINT textureHeight, UINT texturePixelSize)
{
    const UINT rowPitch = textureWidth * texturePixelSize;
    const UINT cellPitch = rowPitch >> 3;        // The width of a cell in the checkboard texture.
    const UINT cellHeight = textureWidth >> 3;    // The height of a cell in the checkerboard texture.
    const UINT textureSize = rowPitch * textureHeight;

    std::vector<UINT8> data(textureSize);
    UINT8* pData = &data[0];

    for (UINT n = 0; n < textureSize; n += texturePixelSize)
    {
        UINT x = n % rowPitch;
        UINT y = n / rowPitch;
        UINT i = x / cellPitch;
        UINT j = y / cellHeight;

        if (i % 2 == j % 2)
        {
            pData[n] = 0x00;        // R
            pData[n + 1] = 0x00;    // G
            pData[n + 2] = 0x00;    // B
            pData[n + 3] = 0xff;    // A
        }
        else
        {
            pData[n] = 0xff;        // R
            pData[n + 1] = 0xff;    // G
            pData[n + 2] = 0xff;    // B
            pData[n + 3] = 0xff;    // A
        }
    }

    return data;
}

Renderer::Renderer(UINT width, UINT height, std::wstring name)
    : m_width(width), m_height(height), m_title(name), m_rtvDescriptorSize(0), m_frameIndex(0),
    m_camera(static_cast<float>(width) / static_cast<float>(height), { 0.0f, 0.0f, -5.0f })
{
    m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

void Renderer::OnInit()
{
    LoadPipeline();
    LoadAssets();
}

void Renderer::OnUpdate()
{
    if (m_inputManager.isKeyDown('W')) m_camera.MoveForward(0.01f);
    if (m_inputManager.isKeyDown('A')) m_camera.MoveRight(-0.01f);
    if (m_inputManager.isKeyDown('S')) m_camera.MoveForward(-0.01f);
    if (m_inputManager.isKeyDown('D')) m_camera.MoveRight(0.01f);
    if (m_inputManager.isKeyDown('Q')) m_camera.MoveUp(-0.01f);
    if (m_inputManager.isKeyDown('E')) m_camera.MoveUp(0.01f);

    XMINT2 mouseMove = m_inputManager.GetAndResetMouseMove();
    m_camera.Rotate(mouseMove);

    // 이번에 드로우할 프레임에 대해 constant buffers 업데이트
    FrameResource* pFrameResource = m_frameResources[m_frameIndex];

    for (auto* pMesh : m_meshes)
    {
        XMMATRIX world = XMMatrixTranslation(1.0f, 0.0f, -1.0f);
        XMStoreFloat4x4(&pMesh->m_meshBufferData.world, XMMatrixTranspose(world));
        world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMStoreFloat4x4(&pMesh->m_meshBufferData.inverseTranspose, XMMatrixInverse(nullptr, world));
        pFrameResource->m_meshConstantBuffers[pMesh->m_meshConstantBufferIndex]->Update(&pMesh->m_meshBufferData);

        pMesh->m_materialConstantBufferData.materialAmbient = { 0.1f, 0.1f, 0.1f };
        pMesh->m_materialConstantBufferData.materialSpecular = { 1.0f, 1.0f, 1.0f };
        pMesh->m_materialConstantBufferData.shininess = 10.0f;
        pFrameResource->m_materialConstantBuffers[pMesh->m_materialConstantBufferIndex]->Update(&pMesh->m_materialConstantBufferData);
    }

    for (auto* pMesh : m_instancedMeshes)
    {
        XMMATRIX world = XMMatrixIdentity();
        XMStoreFloat4x4(&pMesh->m_meshBufferData.world, XMMatrixTranspose(world));
        world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMStoreFloat4x4(&pMesh->m_meshBufferData.inverseTranspose, XMMatrixInverse(nullptr, world));
        pFrameResource->m_meshConstantBuffers[pMesh->m_meshConstantBufferIndex]->Update(&pMesh->m_meshBufferData);

        pMesh->m_materialConstantBufferData.materialAmbient = { 0.1f, 0.1f, 0.1f };
        pMesh->m_materialConstantBufferData.materialSpecular = { 1.0f, 1.0f, 1.0f };
        pMesh->m_materialConstantBufferData.shininess = 10.0f;
        pFrameResource->m_materialConstantBuffers[pMesh->m_materialConstantBufferIndex]->Update(&pMesh->m_materialConstantBufferData);
    }

    m_lightConstantData.lightPos = { 0.0f, 100.0f, 0.0f };
    m_lightConstantData.lightDir = { -1.0f, -1.0f, 1.0f };
    m_lightConstantData.lightColor = { 1.0f, 1.0f, 1.0f };
    m_lightConstantData.lightIntensity = 1.0f;
    pFrameResource->m_lightConstantBuffer->Update(&m_lightConstantData);

    m_cameraConstantData.cameraPos = m_camera.GetPosition();
    XMStoreFloat4x4(&m_cameraConstantData.viewProjection, XMMatrixTranspose(m_camera.GetViewMatrix() * m_camera.GetProjectionMatrix(true)));
    pFrameResource->m_cameraConstantBuffer->Update(&m_cameraConstantData);
}

// Render the scene.
void Renderer::OnRender()
{
    // Record all the commands we need to render the scene into the command list.
    PopulateCommandList();

    // Execute the command list.
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    MoveToNextFrame();
}

void Renderer::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForGPU();

    for (auto* pFrameResource : m_frameResources)
        delete pFrameResource;
    for (auto* pMesh : m_meshes)
        delete pMesh;

    CloseHandle(m_fenceEvent);
}

void Renderer::OnKeyDown(WPARAM key)
{
    m_inputManager.SetKeyDown(key);
}

void Renderer::OnKeyUp(WPARAM key)
{
    m_inputManager.SetKeyUp(key);
}

void Renderer::OnMouseMove(int xPos, int yPos)
{
    m_inputManager.CalcMouseMove(xPos, yPos);
}

void Renderer::OnResize(UINT width, UINT height)
{
    if (width == m_width && height == m_height) return;

    // Wait till GPU complete currently queued works
    WaitForGPU();

    m_width = width;
    m_height = height;

    m_viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
    m_camera.SetAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));

    // Release resources
    for (UINT i = 0; i < FrameCount; i++)
        m_frameResources[i]->m_renderTarget.Reset();
    m_depthStencil.Reset();

    // Preserve existing format
    m_swapChain->ResizeBuffers(FrameCount, m_width, m_height, DXGI_FORMAT_UNKNOWN, 0);
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate RTVs
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FrameCount; i++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_frameResources[i]->m_renderTarget)));
        m_device->CreateRenderTargetView(m_frameResources[i]->m_renderTarget.Get(), nullptr, rtvHandle);
        MoveCPUDescriptorHandle(&rtvHandle, 1, m_rtvDescriptorSize);
    }

    // Recreate DSV
    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
    depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
    depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
    depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
    depthOptimizedClearValue.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES heapProperties = {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = m_width;
    resourceDesc.Height = m_height;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
    resourceDesc.SampleDesc = { 1, 0 };
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &depthOptimizedClearValue,
        IID_PPV_ARGS(&m_depthStencil)
    ));

    m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilViewDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Renderer::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the D3D12 debug layer and set the DXGI debug flag
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    // Create factory
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    // Create device using appropriate adapter
    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }

    // Describe and create the command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        swapChain.GetAddressOf()
    ));

    // GetCurrentBackBufferIndex을 사용하기 위해 IDXGISwapChain3로 쿼리
    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // fullscreen transition
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_VALID));

    // Create descriptor heaps
    {
        // RTV descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        // CBV_SRV_UAV descriptor heap is managed by DescriptorHeapManager
        m_cbvSrvUavHeap.Init(m_device, FrameCount, MaxDynamicCbvCountPerFrame, MaxStaticSrvCount);

        // DSV descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources
    {
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV and command allocator for each frame
        for (UINT i = 0; i < FrameCount; i++)
        {
            FrameResource* pFrameResource = new FrameResource(m_device, m_swapChain, i, rtvHandle);
            MoveCPUDescriptorHandle(&rtvHandle, 1, m_rtvDescriptorSize);
            m_frameResources.push_back(pFrameResource);
        }
    }

    // 프레임에 독립적인 command allocator를 하나 생성. 프레임과 무관한 작업을 할 때 사용
    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
    //ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&m_bundleAllocator)));
}

// Load the sample assets.
void Renderer::LoadAssets()
{
    // Create root signature
    {
        // Use D3D_ROOT_SIGNATURE_VERSION_1_1 if current environment supports it
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
        {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // idx 0 for per-mesh CBV, 1 for default CBV, 2 for SRV
        D3D12_DESCRIPTOR_RANGE1 ranges[3];
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[0].NumDescriptors = 2;   // Mesh + Material
        ranges[0].BaseShaderRegister = 0;
        ranges[0].RegisterSpace = 0;
        ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[1].NumDescriptors = 2;   // Light + Camera
        ranges[1].BaseShaderRegister = 2;
        ranges[1].RegisterSpace = 0;
        ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        ranges[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[2].NumDescriptors = 1;
        ranges[2].BaseShaderRegister = 0;
        ranges[2].RegisterSpace = 0;
        ranges[2].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        ranges[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // idx 0 for per-mesh CBV, 1 for default CBV, 2 for SRV
        D3D12_ROOT_PARAMETER1 rootParameters[3];
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].DescriptorTable = { 1, &ranges[0] };
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[1].DescriptorTable = { 1, &ranges[1] };
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[2].DescriptorTable = { 1, &ranges[2] };
        rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        D3D12_STATIC_SAMPLER_DESC sampler = {};
        sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        sampler.MipLODBias = 0;
        sampler.MaxAnisotropy = 0;
        sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
        sampler.MinLOD = 0.0f;
        sampler.MaxLOD = D3D12_FLOAT32_MAX;
        sampler.ShaderRegister = 0;
        sampler.RegisterSpace = 0;
        sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        // Downgraded objects must not be destroyed until CreateRootSignature
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        D3D12_ROOT_PARAMETER downgradedRootParameters[_countof(rootParameters)];
        std::vector<D3D12_DESCRIPTOR_RANGE> convertedRanges;
        if (featureData.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_1)
        {
            rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
            rootSignatureDesc.Desc_1_1.NumParameters = _countof(rootParameters);
            rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
            rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
            rootSignatureDesc.Desc_1_1.pStaticSamplers = &sampler;
            rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        }
        else
        {
            UINT offset = 0;
            DowngradeRootParameters(rootParameters, _countof(rootParameters), downgradedRootParameters, convertedRanges, offset);

            rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
            rootSignatureDesc.Desc_1_0.NumParameters = _countof(rootParameters);
            rootSignatureDesc.Desc_1_0.pParameters = downgradedRootParameters;
            rootSignatureDesc.Desc_1_0.NumStaticSamplers = 1;
            rootSignatureDesc.Desc_1_0.pStaticSamplers = &sampler;
            rootSignatureDesc.Desc_1_0.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        }

        ComPtr<ID3DBlob> signature;
        ComPtr<ID3DBlob> error;
        ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSignatureDesc, &signature, &error));
        ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
    }

    // Create the pipeline state, which includes compiling and loading shaders.
    {
        ComPtr<ID3DBlob> vertexShader;
        ComPtr<ID3DBlob> instancedVertexShader;
        ComPtr<ID3DBlob> pixelShader;

        UINT compileFlags = 0;
#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        std::wstring vsName = L"vs.hlsl";
        std::wstring psName = L"ps.hlsl";

        // conditional compilation for instancing
        D3D_SHADER_MACRO shaderMacros[] = { { "INSTANCED", "1" }, { NULL, NULL } };
        ThrowIfFailed(D3DCompileFromFile(vsName.c_str(), nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(vsName.c_str(), shaderMacros, nullptr, "main", "vs_5_0", compileFlags, 0, &instancedVertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(psName.c_str(), nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs0[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        D3D12_INPUT_ELEMENT_DESC inputElementDescs1[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

            { "INSTANCE_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },

            { "INSTANCE_INVTRANSPOSE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_INVTRANSPOSE", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 80, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_INVTRANSPOSE", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 96, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_INVTRANSPOSE", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 112, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        };

        // Rasterizer State
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        rasterizerDesc.FrontCounterClockwise = FALSE;
        rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable = TRUE;
        rasterizerDesc.MultisampleEnable = FALSE;
        rasterizerDesc.AntialiasedLineEnable = FALSE;
        rasterizerDesc.ForcedSampleCount = 0;
        rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Blend State
        D3D12_BLEND_DESC blendDesc = {};
        blendDesc.AlphaToCoverageEnable = FALSE;
        blendDesc.IndependentBlendEnable = FALSE;
        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;

        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable = FALSE;
        depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
        { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        depthStencilDesc.FrontFace = defaultStencilOp;
        depthStencilDesc.BackFace = defaultStencilOp;

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs0, _countof(inputElementDescs0) };   // std::size()를 대신 쓰기?
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_defaultPipelineState)));

        psoDesc.VS = { instancedVertexShader->GetBufferPointer(), instancedVertexShader->GetBufferSize() };
        psoDesc.InputLayout = { inputElementDescs1, _countof(inputElementDescs1) };
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_instancedPipelineState)));
    }

    // Create the depth stencil view
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc = {};
        depthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
        depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        depthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;

        D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
        depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        depthOptimizedClearValue.DepthStencil.Depth = 1.0f;
        depthOptimizedClearValue.DepthStencil.Stencil = 0;

        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = m_width;
        resourceDesc.Height = m_height;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_D32_FLOAT;
        resourceDesc.SampleDesc = { 1, 0 };
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &depthOptimizedClearValue,
            IID_PPV_ARGS(&m_depthStencil)
        ));

        m_device->CreateDepthStencilView(m_depthStencil.Get(), &depthStencilViewDesc, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_defaultPipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Create and record the bundle
    {
        //ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, m_bundleAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_bundle)));
        // 번들은 정적 디스크립터 혹은 정적 데이터를 가리키는 디스크립터를 갖는 파라미터가 포함된 루트 시그니처를 상속받지 못한다.
        // 루트 시그니처 내에 D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC 플래그를 사용하는 SRV가 있기 때문에 루트 시그니처를 번들에서 다시 설정해줘야 한다. 
        //m_bundle->SetGraphicsRootSignature(m_rootSignature.Get());
        //m_bundle->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        //m_bundle->IASetVertexBuffers(0, 1, &m_vertexBufferView);
        //m_bundle->DrawInstanced(3, 1, 0, 0);
        //ThrowIfFailed(m_bundle->Close());
    }

    // 프레임과 무관한 정적 데이터를 먼저 생성
    ComPtr<ID3D12Resource> vertexBufferUploadHeap0;
    ComPtr<ID3D12Resource> indexBufferUploadHeap0;
    ComPtr<ID3D12Resource> instanceUploadHeap;

    InstancedMesh* pCube = new InstancedMesh(InstancedMesh::MakeCubeInstanced(
        m_device,
        m_commandList,
        vertexBufferUploadHeap0,
        indexBufferUploadHeap0,
        instanceUploadHeap));
    m_instancedMeshes.push_back(pCube);

    ComPtr<ID3D12Resource> vertexBufferUploadHeap1;
    ComPtr<ID3D12Resource> indexBufferUploadHeap1;

    Mesh* pSphere = new Mesh(Mesh::MakeSphere(
        m_device,
        m_commandList,
        vertexBufferUploadHeap1,
        indexBufferUploadHeap1));
    m_meshes.push_back(pSphere);

    // Create constant buffers for each frame
    for (UINT i = 0; i < FrameCount; i++)
    {
        FrameResource* pFrameResource = m_frameResources[i];

        // Meshes
        {
            for (auto* pMesh : m_meshes)
            {
                // 디스크립터 힙 내 per-frame 디스크립터의 상대적인 오프셋 설정은 첫 한번만 수행해야 함
                if (i == 0)
                    pMesh->m_perMeshCbvDescriptorOffset = m_cbvSrvUavHeap.GetNumCbvAllocated();

                // Mesh
                {
                    MeshCB* meshCB = new MeshCB(m_device, m_cbvSrvUavHeap.GetFreeHandleForCbv());
                    meshCB->Update(&pMesh->m_meshBufferData);

                    // 각 FrameResource에서 동일한 인덱스긴 하지만, 한 번만 수행하도록 하였음
                    if (i == 0)
                        pMesh->m_meshConstantBufferIndex = UINT(pFrameResource->m_meshConstantBuffers.size());
                    pFrameResource->m_meshConstantBuffers.push_back(meshCB);
                }

                // Material
                {
                    MaterialCB* materialCB = new MaterialCB(m_device, m_cbvSrvUavHeap.GetFreeHandleForCbv());
                    materialCB->Update(&pMesh->m_materialConstantBufferData);

                    if (i == 0)
                        pMesh->m_materialConstantBufferIndex = UINT(pFrameResource->m_materialConstantBuffers.size());
                    pFrameResource->m_materialConstantBuffers.push_back(materialCB);
                }
            }

            for (auto* pMesh : m_instancedMeshes)
            {
                if (i == 0)
                    pMesh->m_perMeshCbvDescriptorOffset = m_cbvSrvUavHeap.GetNumCbvAllocated();

                // Mesh
                {
                    MeshCB* meshCB = new MeshCB(m_device, m_cbvSrvUavHeap.GetFreeHandleForCbv());
                    meshCB->Update(&pMesh->m_meshBufferData);

                    if (i == 0)
                        pMesh->m_meshConstantBufferIndex = UINT(pFrameResource->m_meshConstantBuffers.size());
                    pFrameResource->m_meshConstantBuffers.push_back(meshCB);
                }

                // Material
                {
                    MaterialCB* materialCB = new MaterialCB(m_device, m_cbvSrvUavHeap.GetFreeHandleForCbv());
                    materialCB->Update(&pMesh->m_materialConstantBufferData);

                    if (i == 0)
                        pMesh->m_materialConstantBufferIndex = UINT(pFrameResource->m_materialConstantBuffers.size());
                    pFrameResource->m_materialConstantBuffers.push_back(materialCB);
                }
            }
        }

        // 모든 메쉬에서 공통으로 사용하는 CBV에 대한 오프셋
        if (i == 0)
        {
            UINT idx = m_cbvSrvUavHeap.GetNumCbvAllocated();

            for (auto* pMesh : m_meshes)
                pMesh->m_defaultCbvDescriptorOffset = idx;
            for (auto* pMesh : m_instancedMeshes)
                pMesh->m_defaultCbvDescriptorOffset = idx;
        }

        // Lights
        {
            LightCB* lightCB = new LightCB(m_device, m_cbvSrvUavHeap.GetFreeHandleForCbv());
            lightCB->Update(&m_lightConstantData);

            pFrameResource->m_lightConstantBuffer = lightCB;
        }

        // Camera
        {
            CameraCB* cameraCB = new CameraCB(m_device, m_cbvSrvUavHeap.GetFreeHandleForCbv());
            cameraCB->Update(&m_cameraConstantData);

            pFrameResource->m_cameraConstantBuffer = cameraCB;
        }

        if (i == 0)
            m_cbvSrvUavHeap.m_numCbvPerFrame = m_cbvSrvUavHeap.GetNumCbvAllocated();
    }

    ComPtr<ID3D12Resource> textureUploadHeap;

    std::vector<UINT8> simpleTextureData = GenerateTextureData(256, 256, 4);

    // 텍스처 생성, Mesh에 할당
    UINT idx = m_cbvSrvUavHeap.GetNumSrvAllocated();
    CreateTexture(m_device, m_commandList, m_texture, textureUploadHeap, simpleTextureData, 256, 256, m_cbvSrvUavHeap.GetFreeHandleForSrv());
    pCube->m_srvDescriptorOffset = idx;
    pSphere->m_srvDescriptorOffset = idx;

    ThrowIfFailed(m_commandList->Close());

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_frameResources[m_frameIndex]->m_fenceValue++;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForGPU();
    }
}

void Renderer::PopulateCommandList()
{
    FrameResource* pFrameResource = m_frameResources[m_frameIndex];

    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(pFrameResource->m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    // PSO는 미설정
    ThrowIfFailed(m_commandList->Reset(pFrameResource->m_commandAllocator.Get(), nullptr));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvUavHeap.m_heap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    D3D12_RESOURCE_BARRIER barrier = GetTransitionBarrier(pFrameResource->m_renderTarget, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    MoveCPUDescriptorHandle(&rtvHandle, m_frameIndex, m_rtvDescriptorSize);
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    m_commandList->SetPipelineState(m_defaultPipelineState.Get());
    for (const auto* pMesh : m_meshes)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle;

        // per-mesh CBVs
        // 각 프레임이 사용하는 디스크립터를 인덱싱
        handle = m_cbvSrvUavHeap.GetCbvHandle(m_frameIndex, pMesh->m_perMeshCbvDescriptorOffset);
        m_commandList->SetGraphicsRootDescriptorTable(0, handle);

        // default CBV
        handle = m_cbvSrvUavHeap.GetCbvHandle(0, pMesh->m_defaultCbvDescriptorOffset);
        m_commandList->SetGraphicsRootDescriptorTable(1, handle);

        // SRV
        handle = m_cbvSrvUavHeap.GetSrvHandle(pMesh->m_srvDescriptorOffset);
        m_commandList->SetGraphicsRootDescriptorTable(2, handle);

        pMesh->Render(m_commandList);
    }

    m_commandList->SetPipelineState(m_instancedPipelineState.Get());
    for (const auto* pMesh : m_instancedMeshes)
    {
        D3D12_GPU_DESCRIPTOR_HANDLE handle;

        // per-mesh CBVs
        // 각 프레임이 사용하는 디스크립터를 인덱싱
        handle = m_cbvSrvUavHeap.GetCbvHandle(m_frameIndex, pMesh->m_perMeshCbvDescriptorOffset);
        m_commandList->SetGraphicsRootDescriptorTable(0, handle);

        // default CBV
        handle = m_cbvSrvUavHeap.GetCbvHandle(0, pMesh->m_defaultCbvDescriptorOffset);
        m_commandList->SetGraphicsRootDescriptorTable(1, handle);

        // SRV
        handle = m_cbvSrvUavHeap.GetSrvHandle(pMesh->m_srvDescriptorOffset);
        m_commandList->SetGraphicsRootDescriptorTable(2, handle);

        pMesh->Render(m_commandList);
    }

    // Indicate that the back buffer will now be used to present.
    barrier = GetTransitionBarrier(pFrameResource->m_renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
    m_commandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_commandList->Close());
}

// Wait for pending GPU work to complete
void Renderer::WaitForGPU()
{
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_frameResources[m_frameIndex]->m_fenceValue));

    ThrowIfFailed(m_fence->SetEventOnCompletion(m_frameResources[m_frameIndex]->m_fenceValue, m_fenceEvent));
    WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

    m_frameResources[m_frameIndex]->m_fenceValue++;
}

void Renderer::MoveToNextFrame()
{
    // 이전 프레임에 대한 fence 값
    const UINT64 currentFenceValue = m_frameResources[m_frameIndex]->m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

    // 현재 프레임 인덱스. Present를 했기 때문에 인덱스가 업데이트되어있다.
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // 현재 프레임에 대한 GPU 작업이 끝나 있는지, 즉 작업할 준비가 되어있는지 검사
    if (m_fence->GetCompletedValue() < m_frameResources[m_frameIndex]->m_fenceValue)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_frameResources[m_frameIndex]->m_fenceValue, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    // 현재 프레임에 대한 목표 fence 값 증가
    m_frameResources[m_frameIndex]->m_fenceValue = currentFenceValue + 1;
}