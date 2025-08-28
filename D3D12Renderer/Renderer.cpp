#include "Renderer.h"
#include "Win32Application.h"
#include "D3DHelper.h"
#include <D3Dcompiler.h>
#include <string>
#include <vector>

// 이거 일단 빼야겠다. nuget에서도 제외시키자
//#include <dxcapi.h>
//#include <d3d12shader.h>

std::vector<UINT8> GenerateTextureData(UINT textureWidth, UINT textureHeight, UINT texturePixelSize);

Renderer::Renderer(UINT width, UINT height, std::wstring name)
    : m_width(width), m_height(height), m_title(name), m_rtvDescriptorSize(0) {
    m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height) };
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };
}

Renderer::~Renderer() {

}

void Renderer::OnInit() {
    LoadPipeline();
    LoadAssets();
}

void Renderer::OnUpdate() {

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

    WaitForPreviousFrame();
}

void Renderer::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForPreviousFrame();

    CloseHandle(m_fenceEvent);
}

// Helper function for acquiring the first available hardware adapter that supports Direct3D 12.
// If no such adapter can be found, *ppAdapter will be set to nullptr.
void Renderer::GetHardwareAdapter(
    _In_ IDXGIFactory1* pFactory,
    _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
    bool requestHighPerformanceAdapter)
{
    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (
            UINT adapterIndex = 0;
            SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                adapterIndex,
                requestHighPerformanceAdapter == true ? DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE : DXGI_GPU_PREFERENCE_UNSPECIFIED,
                IID_PPV_ARGS(&adapter)));
                ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    if (adapter.Get() == nullptr)
    {
        for (UINT adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't create the
            // actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
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

        // Describe and create a shader resource view (SRV) descriptor heap
        // for texture
        D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
        srvHeapDesc.NumDescriptors = 1;
        srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

        m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // Create frame resources
    {
        //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        // Create a RTV for each frame.
        for (UINT n = 0; n < FrameCount; n++)
        {
            ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
            m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
            //rtvHandle.Offset(1, m_rtvDescriptorSize);
            rtvHandle.ptr = SIZE_T(INT64(rtvHandle.ptr) + INT64(m_rtvDescriptorSize));
            //ptr = SIZE_T(INT64(ptr) + INT64(offsetInDescriptors) * INT64(descriptorIncrementSize));
        }
    }

    ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));
}

// Load the sample assets.
void Renderer::LoadAssets()
{
    // Create root signature
    {
        // Create an empty root signature.
        //D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
        //rootSignatureDesc.NumParameters = 0;
        //rootSignatureDesc.pParameters = nullptr;
        //rootSignatureDesc.NumStaticSamplers = 0;
        //rootSignatureDesc.pStaticSamplers = nullptr;
        //rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        //ComPtr<ID3DBlob> signature;
        //ComPtr<ID3DBlob> error;
        //ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
        //ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

        // Use D3D_ROOT_SIGNATURE_VERSION_1_1 if current environment supports it
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        D3D12_DESCRIPTOR_RANGE1 ranges[1];
        ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        ranges[0].NumDescriptors = 1;
        ranges[0].BaseShaderRegister = 0;
        ranges[0].RegisterSpace = 0;
        ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC;
        ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

        D3D12_ROOT_PARAMETER1 rootParameters[1];
        rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParameters[0].DescriptorTable = { 1, &ranges[0] };
        rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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
        D3D12_ROOT_PARAMETER downgradedRootParameters[1];
        std::vector<D3D12_DESCRIPTOR_RANGE> convertedRanges;
        if (featureData.HighestVersion == D3D_ROOT_SIGNATURE_VERSION_1_1) {
            rootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
            rootSignatureDesc.Desc_1_1.NumParameters = _countof(rootParameters);
            rootSignatureDesc.Desc_1_1.pParameters = rootParameters;
            rootSignatureDesc.Desc_1_1.NumStaticSamplers = 1;
            rootSignatureDesc.Desc_1_1.pStaticSamplers = &sampler;
            rootSignatureDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        }
        else {
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
            //{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };   // std::size()를 대신 쓰기?
        psoDesc.pRootSignature = m_rootSignature.Get();
        psoDesc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
        psoDesc.PS = { pixelShader->GetBufferPointer(), pixelShader->GetBufferSize() };
        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState.DepthEnable = FALSE;
        psoDesc.DepthStencilState.StencilEnable = FALSE;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
    }

    // Create the command list.
    ThrowIfFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), m_pipelineState.Get(), IID_PPV_ARGS(&m_commandList)));

    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] =
        {
            { { 0.0f, 0.25f * m_aspectRatio, 0.0f }, { 0.5f, 0.0f } },
            { { 0.25f, -0.25f * m_aspectRatio, 0.0f }, { 1.0f, 1.0f } },
            { { -0.25f, -0.25f * m_aspectRatio, 0.0f }, { 0.0f, 1.0f } }
        };

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not 
        // recommended. Every time the GPU needs it, the upload heap will be marshalled 
        // over. Please read up on Default Heap usage. An upload heap is used here for 
        // code simplicity and because there are very few verts to actually transfer.

        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = vertexBufferSize;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc = { 1, 0 };
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        D3D12_RANGE readRange = { 0, 0 };       // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }

    // Note: ComPtr's are CPU objects but this resource needs to stay in scope until
    // the command list that references it has finished executing on the GPU.
    // We will flush the GPU at the end of this method to ensure the resource is not
    // prematurely destroyed.
    ComPtr<ID3D12Resource> textureUploadHeap;

    // Create the texture.
    {
        D3D12_HEAP_PROPERTIES heapProperties = {};
        heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        heapProperties.CreationNodeMask = 1;
        heapProperties.VisibleNodeMask = 1;

        // Describe and create a Texture2D.
        D3D12_RESOURCE_DESC textureDesc = {};
        textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        textureDesc.Alignment = 0;
        textureDesc.Width = TextureWidth;
        textureDesc.Height = TextureHeight;
        textureDesc.DepthOrArraySize = 1;
        textureDesc.MipLevels = 1;
        textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        textureDesc.SampleDesc = { 1, 0 };
        textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &textureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&m_texture)));

        // Calculate required size for data upload
        //const auto desc = m_texture->GetDesc();
        // 나중에 Texture2D 클래스를 만들어서 이 정보들을 멤버로 유지하도록 하자
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layouts = {};
        UINT numRows = 0;
        UINT64 rowSizeInBytes = 0;
        UINT64 requiredSize = 0;
        //ID3D12Device* pDevice = nullptr;
        //m_texture->GetDevice(IID_ID3D12Device, reinterpret_cast<void**>(&pDevice));
        m_device->GetCopyableFootprints(&textureDesc, 0, 1, 0, &layouts, &numRows, &rowSizeInBytes, &requiredSize);
        //pDevice->Release();

        heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC uploadDesc = {};
        uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        uploadDesc.Alignment = 0;
        uploadDesc.Width = requiredSize;
        uploadDesc.Height = 1;
        uploadDesc.DepthOrArraySize = 1;
        uploadDesc.MipLevels = 1;
        uploadDesc.Format = DXGI_FORMAT_UNKNOWN;
        uploadDesc.SampleDesc = { 1, 0 };
        uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        uploadDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        // Create the GPU upload buffer.
        ThrowIfFailed(m_device->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&textureUploadHeap)));

        // Copy data to the intermediate upload heap and then schedule a copy 
        // from the upload heap to the Texture2D.
        std::vector<UINT8> texture = GenerateTextureData(TextureWidth, TextureHeight, TexturePixelSize);

        D3D12_SUBRESOURCE_DATA textureData = {};
        textureData.pData = &texture[0];
        textureData.RowPitch = TextureWidth * TexturePixelSize;
        textureData.SlicePitch = textureData.RowPitch * TextureHeight;

        //UpdateSubresources(m_commandList.Get(), m_texture.Get(), textureUploadHeap.Get(), 0, 0, 1, &textureData);
        D3D12_RANGE readRange = { 0, 0 };   // do not read from CPU. only write
        UINT8* pData;
        ThrowIfFailed(textureUploadHeap->Map(0, &readRange, reinterpret_cast<void**>(&pData)));
        //memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));

        // Assume that NumSubResources is 1
        // Data need for memcpy
        //D3D12_MEMCPY_DEST destData = {
        //    pData + layouts.Offset,
        //    layouts.Footprint.RowPitch,
        //    SIZE_T(layouts.Footprint.RowPitch) * SIZE_T(numRows)
        //};

        for (UINT z = 0; z < layouts.Footprint.Depth; ++z)
        {
            auto pDestSlice = static_cast<BYTE*>(pData + layouts.Offset) + SIZE_T(layouts.Footprint.RowPitch) * SIZE_T(numRows) * z;
            auto pSrcSlice = static_cast<const BYTE*>(textureData.pData) + textureData.SlicePitch * LONG_PTR(z);
            for (UINT y = 0; y < numRows; ++y)
            {
                memcpy(pDestSlice + layouts.Footprint.RowPitch * y,
                    pSrcSlice + textureData.RowPitch * LONG_PTR(y),
                    rowSizeInBytes);
            }
        }

        textureUploadHeap->Unmap(0, nullptr);

        // Copy from upload heap to default heap
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = m_texture.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = textureUploadHeap.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts;

        m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        // Change resource state
        D3D12_RESOURCE_BARRIER barrier = {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_texture.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        m_commandList->ResourceBarrier(1, &barrier);

        // Describe and create a SRV for the texture.
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        m_device->CreateShaderResourceView(m_texture.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
    }

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Create synchronization objects and wait until assets have been uploaded to the GPU.
    {
        ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
        m_fenceValue = 1;

        // Create an event handle to use for frame synchronization.
        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if (m_fenceEvent == nullptr)
        {
            ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
        }

        // Wait for the command list to execute; we are reusing the same command 
        // list in our main loop but for now, we just want to wait for setup to 
        // complete before continuing.
        WaitForPreviousFrame();
    }
}

void Renderer::PopulateCommandList()
{
    // Command list allocators can only be reset when the associated 
    // command lists have finished execution on the GPU; apps should use 
    // fences to determine GPU execution progress.
    ThrowIfFailed(m_commandAllocator->Reset());

    // However, when ExecuteCommandList() is called on a particular command 
    // list, that command list can then be reset at any time and must be before 
    // re-recording.
    ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Set necessary state.
    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    ID3D12DescriptorHeap* ppHeaps[] = { m_srvHeap.Get() };
    m_commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    m_commandList->SetGraphicsRootDescriptorTable(0, m_srvHeap->GetGPUDescriptorHandleForHeapStart());

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Indicate that the back buffer will be used as a render target.
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE start = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr = SIZE_T(INT64(start.ptr) + INT64(m_frameIndex) * INT64(m_rtvDescriptorSize));
    //CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
    m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

    // Record commands.
    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    m_commandList->DrawInstanced(3, 1, 0, 0);

    // Indicate that the back buffer will now be used to present.
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);

    ThrowIfFailed(m_commandList->Close());
}

void Renderer::WaitForPreviousFrame()
{
    // WAITING FOR THE FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
    // This is code implemented as such for simplicity. The D3D12HelloFrameBuffering
    // sample illustrates how to use fences for efficient resource usage and to
    // maximize GPU utilization.

    // Signal and increment the fence value.
    const UINT64 fence = m_fenceValue;
    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
    m_fenceValue++;

    // Wait until the previous frame is finished.
    if (m_fence->GetCompletedValue() < fence)
    {
        ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void Renderer::DowngradeRootParameters(D3D12_ROOT_PARAMETER1* src, UINT numParameters, D3D12_ROOT_PARAMETER* dst,
    std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges, UINT& offset) {
    for (UINT i = 0; i < numParameters; i++) {
        dst[i].ParameterType = src[i].ParameterType;

        const D3D12_ROOT_PARAMETER_TYPE& type = src[i].ParameterType;
        switch (type) {
        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
            const UINT NumDescriptorRanges = src[i].DescriptorTable.NumDescriptorRanges;
            DowngradeDescriptorRanges(src[i].DescriptorTable.pDescriptorRanges, NumDescriptorRanges, convertedRanges);
            dst[i].DescriptorTable = { NumDescriptorRanges, convertedRanges.data() + offset };
            offset += NumDescriptorRanges;
            break;
        }
        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
            dst[i].Constants = src[i].Constants;
            break;
        }
        case D3D12_ROOT_PARAMETER_TYPE_CBV:
        case D3D12_ROOT_PARAMETER_TYPE_SRV:
        case D3D12_ROOT_PARAMETER_TYPE_UAV: {
            DowngradeRootDescriptor(&src[i].Descriptor, &dst[i].Descriptor);
            break;
        }
        }

        dst[i].ShaderVisibility = src[i].ShaderVisibility;
    }
}
void Renderer::DowngradeDescriptorRanges(const D3D12_DESCRIPTOR_RANGE1* src, UINT NumDescriptorRanges,
    std::vector<D3D12_DESCRIPTOR_RANGE>& convertedRanges) {
    D3D12_DESCRIPTOR_RANGE tempRange = {};
    for (UINT i = 0; i < NumDescriptorRanges; i++) {
        tempRange.RangeType = src[i].RangeType;
        tempRange.NumDescriptors = src[i].NumDescriptors;
        tempRange.BaseShaderRegister = src[i].BaseShaderRegister;
        tempRange.RegisterSpace = src[i].RegisterSpace;
        tempRange.OffsetInDescriptorsFromTableStart = src[i].OffsetInDescriptorsFromTableStart;
        convertedRanges.push_back(tempRange);
    }
}

void Renderer::DowngradeRootDescriptor(D3D12_ROOT_DESCRIPTOR1* src, D3D12_ROOT_DESCRIPTOR* dst) {
    dst->ShaderRegister = src->ShaderRegister;
    dst->RegisterSpace = src->RegisterSpace;
}

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