#include "Renderer.h"
#include "Win32Application.h"
#include "D3DHelper.h"
#include <D3Dcompiler.h>

// 이거 일단 빼야겠다. nuget에서도 제외시키자
//#include <dxcapi.h>
//#include <d3d12shader.h>

using namespace D3DHelper;

Renderer::Renderer(UINT width, UINT height, std::wstring name)
    : m_width(width), m_height(height), m_title(name), m_rtvDescriptorSize(0), m_cbvSrvUavDescriptorSize(0), m_frameIndex(0), m_fenceValues{ 0 },
    m_camera(static_cast<float>(width) / static_cast<float>(height), { 0.0f, 0.0f, -5.0f })
{
    m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

Renderer::~Renderer()
{
    for (auto* pFrameResource : m_frameResources)
        delete pFrameResource;
    for (auto* pMesh : m_meshes)
        delete pMesh;
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
        //XMMATRIX world = XMMatrixRotationY(XMConvertToRadians(25.0f)) * XMMatrixRotationX(XMConvertToRadians(-25.0f));
        XMMATRIX world = XMMatrixIdentity();
        XMStoreFloat4x4(&pMesh->m_constantBufferData.world, XMMatrixTranspose(world));
        XMStoreFloat4x4(&pMesh->m_constantBufferData.view, XMMatrixTranspose(m_camera.GetViewMatrix()));
        XMStoreFloat4x4(&pMesh->m_constantBufferData.projection, XMMatrixTranspose(m_camera.GetProjectionMatrix(true)));
        world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMStoreFloat4x4(&pMesh->m_constantBufferData.inverseTranspose, XMMatrixInverse(nullptr, world));
        memcpy(pFrameResource->m_pSceneBufferBegin, &pMesh->m_constantBufferData, sizeof(SceneConstantData));

        pMesh->m_materialConstantBufferData.materialAmbient = { 0.1f, 0.1f, 0.1f };
        pMesh->m_materialConstantBufferData.materialSpecular = { 1.0f, 1.0f, 1.0f };
        pMesh->m_materialConstantBufferData.shininess = 10.0f;
        memcpy(pFrameResource->m_pMaterialBufferBegin, &pMesh->m_materialConstantBufferData, sizeof(MaterialConstantData));
    }

    m_lightConstantData.lightPos = { 0.0f, 100.0f, 0.0f };
    m_lightConstantData.lightDir = { -1.0f, -1.0f, 1.0f };
    m_lightConstantData.lightColor = { 1.0f, 1.0f, 1.0f };
    m_lightConstantData.lightIntensity = 1.0f;
    memcpy(pFrameResource->m_pLightBufferBegin, &m_lightConstantData, sizeof(LightConstantData));

    m_cameraConstantData.cameraPos = m_camera.GetPosition();
    memcpy(pFrameResource->m_pCameraBufferBegin, &m_cameraConstantData, sizeof(CameraConstantData));
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
        // Describe and create a render target view (RTV) descriptor heap
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
        rtvHeapDesc.NumDescriptors = FrameCount;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        // Describe and create a descriptor heap for CBV, SRV, UAV
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = FrameCount * 4 + 1;     // (light + camera + transform + material) * FrameCount + texture
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_cbvSrvUavHeap)));

        // Depth stencil view heap
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        m_cbvSrvUavDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
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

        // idx 0 for CBV, 1 for SRV
        D3D12_DESCRIPTOR_RANGE1 ranges[2];
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        ranges[0].NumDescriptors = 4;       // light + camera + transform + material
        ranges[0].BaseShaderRegister = 0;
        ranges[0].RegisterSpace = 0;
        ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[1].NumDescriptors = 1;
        ranges[1].BaseShaderRegister = 0;
        ranges[1].RegisterSpace = 0;
        ranges[1].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        // idx 0 for CBV, 1 for SRV
        D3D12_ROOT_PARAMETER1 rootParameters[2];
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].DescriptorTable = { 1, &ranges[0] };
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

        rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[1].DescriptorTable = { 1, &ranges[1] };
        rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
        ComPtr<ID3DBlob> pixelShader;

        UINT compileFlags = 0;
#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        std::wstring vsName = L"vs.hlsl";
        std::wstring psName = L"ps.hlsl";

        ThrowIfFailed(D3DCompileFromFile(vsName.c_str(), nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vertexShader, nullptr));
        ThrowIfFailed(D3DCompileFromFile(psName.c_str(), nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &pixelShader, nullptr));

        // Define the vertex input layout.
        D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
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
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };   // std::size()를 대신 쓰기?
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
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
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
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

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
    ComPtr<ID3D12Resource> vertexBufferUploadHeap;
    ComPtr<ID3D12Resource> indexBufferUploadHeap;
    ComPtr<ID3D12Resource> textureUploadHeap;
    ComPtr<ID3D12Resource> instanceUploadHeap;

    InstancedMesh* cube = new InstancedMesh(InstancedMesh::MakeCubeInstanced(
        m_device,
        m_commandList,
        vertexBufferUploadHeap,
        indexBufferUploadHeap,
        instanceUploadHeap));
    m_meshes.push_back(cube);

    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_cbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();

    // Create constant buffers for each frame
    for (UINT i = 0; i < FrameCount; i++)
    {
        FrameResource* pFrameResource = m_frameResources[i];

        // Meshes
        {
            for (auto* pMesh : m_meshes)
            {
                {
                    // Scene
                    CreateUploadHeap(m_device, sizeof(SceneConstantData), pFrameResource->m_sceneConstantBuffer);

                    // Create constant buffer view
                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                    cbvDesc.BufferLocation = pFrameResource->m_sceneConstantBuffer->GetGPUVirtualAddress();
                    cbvDesc.SizeInBytes = sizeof(SceneConstantData);

                    m_device->CreateConstantBufferView(&cbvDesc, cpuHandle);
                    MoveCPUDescriptorHandle(&cpuHandle, 1, m_cbvSrvUavDescriptorSize);

                    // Do not unmap this until app close
                    D3D12_RANGE readRange = { 0, 0 };
                    ThrowIfFailed(pFrameResource->m_sceneConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pFrameResource->m_pSceneBufferBegin)));
                    memcpy(pFrameResource->m_pSceneBufferBegin, &pMesh->m_constantBufferData, sizeof(SceneConstantData));
                }

                {
                    // Material
                    CreateUploadHeap(m_device, sizeof(MaterialConstantData), pFrameResource->m_materialConstantBuffer);

                    // Create constant buffer view
                    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
                    cbvDesc.BufferLocation = pFrameResource->m_materialConstantBuffer->GetGPUVirtualAddress();
                    cbvDesc.SizeInBytes = sizeof(MaterialConstantData);

                    m_device->CreateConstantBufferView(&cbvDesc, cpuHandle);
                    MoveCPUDescriptorHandle(&cpuHandle, 1, m_cbvSrvUavDescriptorSize);

                    // Do not unmap this until app close
                    D3D12_RANGE readRange = { 0, 0 };
                    ThrowIfFailed(pFrameResource->m_materialConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pFrameResource->m_pMaterialBufferBegin)));
                    memcpy(pFrameResource->m_pMaterialBufferBegin, &pMesh->m_materialConstantBufferData, sizeof(MaterialConstantData));
                }
            }
        }

        // Lights
        {
            CreateUploadHeap(m_device, sizeof(LightConstantData), pFrameResource->m_lightConstantBuffer);

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = pFrameResource->m_lightConstantBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = sizeof(LightConstantData);

            m_device->CreateConstantBufferView(&cbvDesc, cpuHandle);
            MoveCPUDescriptorHandle(&cpuHandle, 1, m_cbvSrvUavDescriptorSize);

            // Do not unmap this until app close
            D3D12_RANGE readRange = { 0, 0 };
            ThrowIfFailed(pFrameResource->m_lightConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pFrameResource->m_pLightBufferBegin)));
            memcpy(pFrameResource->m_pLightBufferBegin, &m_lightConstantData, sizeof(LightConstantData));
        }

        // Camera
        {
            CreateUploadHeap(m_device, sizeof(CameraConstantData), pFrameResource->m_cameraConstantBuffer);

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = pFrameResource->m_cameraConstantBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = sizeof(CameraConstantData);

            m_device->CreateConstantBufferView(&cbvDesc, cpuHandle);
            MoveCPUDescriptorHandle(&cpuHandle, 1, m_cbvSrvUavDescriptorSize);

            // Do not unmap this until app close
            D3D12_RANGE readRange = { 0, 0 };
            ThrowIfFailed(pFrameResource->m_cameraConstantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pFrameResource->m_pCameraBufferBegin)));
            memcpy(pFrameResource->m_pCameraBufferBegin, &m_cameraConstantData, sizeof(CameraConstantData));
        }
    }

    // Create the texture
    // 일단은 디스크립터 힙 마지막에 넣는 방법을 임시로 사용
    {
        CreateDefaultHeapForTexture(m_device, m_texture, TextureWidth, TextureHeight);

        // Calculate required size for data upload
        D3D12_RESOURCE_DESC desc = m_texture->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 requiredSize = 0;
        m_device->GetCopyableFootprints(&desc, 0, 1, 0, &layouts, &numRows, &rowSizeInBytes, &requiredSize);

        CreateUploadHeap(m_device, requiredSize, textureUploadHeap);

        // 텍스처 데이터는 인자로 받도록 수정하기
        std::vector<UINT8> texture = GenerateTextureData(TextureWidth, TextureHeight, TexturePixelSize);
        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &texture[0];
        textureData.RowPitch = TextureWidth * TexturePixelSize;
        textureData.SlicePitch = textureData.RowPitch * TextureHeight;

        UpdateSubResources(m_device, m_commandList, m_texture, textureUploadHeap, &textureData);

        // Change resource state
        D3D12_RESOURCE_BARRIER barrier = GetTransitionBarrier(m_texture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        m_commandList->ResourceBarrier(1, &barrier);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = desc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;

        m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, cpuHandle);
        MoveCPUDescriptorHandle(&cpuHandle, 1, m_cbvSrvUavDescriptorSize);
    }

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
    ThrowIfFailed(m_commandList->Reset(pFrameResource->m_commandAllocator.Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_cbvSrvUavHeap.Get() };
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

    // 각 프레임의 디스크립터를 쓰도록 인덱싱
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    MoveGPUDescriptorHandle(&gpuHandle, m_frameIndex * 4, m_cbvSrvUavDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(0, gpuHandle);
    // 핸들을 다시 처음으로 이동시킨 후, 맨 끝의 SRV 디스크립터를 가리키도록 이동
    gpuHandle = m_cbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
    MoveGPUDescriptorHandle(&gpuHandle, FrameCount * 4, m_cbvSrvUavDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(1, gpuHandle);

    for (const auto* pMesh : m_meshes)
    {
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