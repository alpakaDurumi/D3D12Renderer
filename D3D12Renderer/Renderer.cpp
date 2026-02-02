#include "pch.h"
#include "Renderer.h"

#include <DirectXCollision.h>

#include <D3Dcompiler.h>
#include <dxgidebug.h>
#include <chrono>
#include <filesystem>
#include <shlobj.h>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include "Win32Application.h"
#include "D3DHelper.h"
#include "SharedConfig.h"
#include "GeometryGenerator.h"

// 이거 일단 빼야겠다. nuget에서도 제외시키자
//#include <dxcapi.h>
//#include <d3d12shader.h>

using namespace D3DHelper;

// Definition for static member
Renderer* Renderer::sm_instance = nullptr;

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

// https://devblogs.microsoft.com/pix/taking-a-capture/
static std::wstring GetLatestWinPixGpuCapturerPath_Cpp17()
{
    LPWSTR programFilesPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_ProgramFiles, KF_FLAG_DEFAULT, NULL, &programFilesPath);

    std::filesystem::path pixInstallationPath = programFilesPath;
    pixInstallationPath /= "Microsoft PIX";

    std::wstring newestVersionFound;

    for (auto const& directory_entry : std::filesystem::directory_iterator(pixInstallationPath))
    {
        if (directory_entry.is_directory())
        {
            if (newestVersionFound.empty() || newestVersionFound < directory_entry.path().filename().c_str())
            {
                newestVersionFound = directory_entry.path().filename().c_str();
            }
        }
    }

    if (newestVersionFound.empty())
    {
        throw std::runtime_error("No PIX installation found.");
    }

    return pixInstallationPath / newestVersionFound / L"WinPixGpuCapturer.dll";
}

// Wrappers of callback functions for ImGui SRV descriptor
void Renderer::ImGuiSrvDescriptorAllocate(D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
{
    Renderer::GetInstance()->m_imguiDescriptorAllocator->Allocate(out_cpu_handle, out_gpu_handle);
}

void Renderer::ImGuiSrvDescriptorFree(D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
{
    Renderer::GetInstance()->m_imguiDescriptorAllocator->Free(cpu_handle, gpu_handle);
}

Renderer::Renderer(std::wstring name)
    : m_title(name), m_frameIndex(0), m_camera({ 0.0f, 0.0f, -5.0f })
{
    // Set instance pointer
    sm_instance = this;
}

void Renderer::SetPix()
{
    if (GetModuleHandleW(L"WinPixGpuCapturer.dll") == 0)
    {
        std::wstring path = GetLatestWinPixGpuCapturerPath_Cpp17();
        HMODULE hPixModule = LoadLibraryW(path.c_str());
    }
}

void Renderer::UpdateWidthHeight()
{
    m_camera.SetAspectRatio(static_cast<float>(m_width) / static_cast<float>(m_height));
    m_viewport = { 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
}

void Renderer::ToggleFullScreen()
{
    SetFullScreen(!m_fullScreen);
}

void Renderer::SetFullScreen(bool fullScreen)
{
    if (m_fullScreen != fullScreen)
    {
        m_fullScreen = fullScreen;
        if (m_fullScreen)
        {
            // Before switching to fullscreen mode, save window RECT
            GetWindowRect(Win32Application::GetHwnd(), &m_windowRect);

            // fullscreen borderless
            UINT windowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
            SetWindowLongPtrW(Win32Application::GetHwnd(), GWL_STYLE, windowStyle);

            // Query the name of the nearest display device for the window.
            // This is required to set the fullscreen dimensions of the window
            // when using a multi-monitor setup.
            HMONITOR hMonitor = MonitorFromWindow(Win32Application::GetHwnd(), MONITOR_DEFAULTTONEAREST);
            MONITORINFOEXW monitorInfo = {};
            monitorInfo.cbSize = sizeof(MONITORINFOEXW);
            GetMonitorInfoW(hMonitor, &monitorInfo);

            SetWindowPos(Win32Application::GetHwnd(), HWND_TOP,
                monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(Win32Application::GetHwnd(), SW_MAXIMIZE);
        }
        else
        {
            // Restore all the window decorators.
            SetWindowLongW(Win32Application::GetHwnd(), GWL_STYLE, WS_OVERLAPPEDWINDOW);

            SetWindowPos(Win32Application::GetHwnd(), HWND_NOTOPMOST,
                m_windowRect.left,
                m_windowRect.top,
                m_windowRect.right - m_windowRect.left,
                m_windowRect.bottom - m_windowRect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE);

            ShowWindow(Win32Application::GetHwnd(), SW_NORMAL);
        }
    }
}

void Renderer::OnInit()
{
    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));   // For initializing DirectXTex
    LoadPipeline();
    LoadAssets();
    InitImGui();
}

void Renderer::OnUpdate()
{
    PrintFPS();
    HandleInput();

    // 이번에 드로우할 프레임에 대해 constant buffers 업데이트
    FrameResource& frameResource = *m_frameResources[m_frameIndex];

    PrepareConstantData();
    UpdateConstantBuffers(frameResource);
}

// Render the scene.
void Renderer::OnRender()
{
    auto [commandAllocator, commandList] = m_commandQueue->GetAvailableCommandList();

    // Record all the commands we need to render the scene into the command list
    PopulateCommandList(commandList);

    m_dynamicDescriptorHeap->Reset();

    // ImGui Render
    // Is it OK to call SetDescriptorHeaps? (Does it affect performance?)
    ImGui::Render();
    ID3D12DescriptorHeap* ppHeaps[] = { m_imguiDescriptorAllocator->GetDescriptorHeap() };
    commandList.GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.GetCommandList().Get());

    // Barrier for RTV should be called after ImGui Render.
    // Swap Chain textures initially created in D3D12_BARRIER_LAYOUT_COMMON.
    // and presentation requires the back buffer is using D3D12_BARRIER_LAYOUT_COMMON.
    // LAYOUT_PRESENT is alias for LAYOUT_COMMON.
    commandList.Barrier(
        m_frameResources[m_frameIndex]->m_renderTarget.Get(),
        D3D12_BARRIER_SYNC_RENDER_TARGET,
        D3D12_BARRIER_SYNC_NONE,
        D3D12_BARRIER_ACCESS_RENDER_TARGET,
        D3D12_BARRIER_ACCESS_NO_ACCESS,
        D3D12_BARRIER_LAYOUT_PRESENT);

    // Execute the command lists and store the fence value
    // And notify fenceValue to UploadBuffer
    UINT64 fenceValue = m_commandQueue->ExecuteCommandLists(commandAllocator, commandList, *m_layoutTracker);
    m_frameResources[m_frameIndex]->m_fenceValue = fenceValue;
    m_uploadBuffer->QueueRetiredPages(fenceValue);
    m_dynamicDescriptorHeap->QueueRetiredHeaps(fenceValue);

    // Present the frame.
    UINT syncInterval = m_vSync ? 1 : 0;
    UINT presentFlags = m_tearingSupported && !m_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(m_swapChain->Present(syncInterval, presentFlags));

    m_inputManager.OnFrameEnd();

    MoveToNextFrame();
}

void Renderer::OnDestroy()
{
    // Ensure that the GPU is no longer referencing resources that are about to be
    // cleaned up by the destructor.
    WaitForGPU();

    // Shutdown ImGui
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
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

    // Size 0 is not allowed
    m_width = std::max(1u, width);
    m_height = std::max(1u, height);
    UpdateWidthHeight();

    // Unregister and release current render target, set frame fence values to the current fence value
    for (UINT i = 0; i < FrameCount; i++)
    {
        m_layoutTracker->UnregisterResource(m_frameResources[i]->m_renderTarget.Get());
        m_frameResources[i]->m_renderTarget.Reset();
        m_frameResources[i]->m_fenceValue = m_frameResources[m_frameIndex]->m_fenceValue;
    }
    m_depthStencilBuffer.Reset();

    // Preserve existing format
    // Before calling ResizeBuffers, all backbuffer references should be released.
    ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, m_width, m_height, DXGI_FORMAT_UNKNOWN, m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate RTVs
    for (UINT i = 0; i < FrameCount; i++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_frameResources[i]->m_renderTarget)));

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        m_device->CreateRenderTargetView(m_frameResources[i]->m_renderTarget.Get(), &rtvDesc, m_frameResources[i]->m_rtvAllocation.GetDescriptorHandle());

        // Assume that ResizeBuffers do not preserve previous layout.
        // For now, just use D3D12_BARRIER_LAYOUT_COMMON.
        auto desc = m_frameResources[i]->m_renderTarget->GetDesc();
        m_layoutTracker->RegisterResource(m_frameResources[i]->m_renderTarget.Get(), D3D12_BARRIER_LAYOUT_COMMON, desc.DepthOrArraySize, desc.MipLevels, DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    // Recreate DSV
    CreateDepthStencilBuffer(m_device.Get(), m_width, m_height, m_depthStencilBuffer, m_dsvAllocation);
}

void Renderer::OnPrepareImGui()
{
    //ImGui::ShowDemoWindow(); // Show demo window! :)

    ImGui::Begin("Test");

    const char* items[] = { "Point", "Bilinear", "AnisotropicX2", "AnisotropicX4", "AnisotropicX8", "AnisotropicX16" };
    static int item_selected_idx = 5;

    const char* combo_preview_value = items[item_selected_idx];
    if (ImGui::BeginCombo("Texture Filtering", combo_preview_value))
    {
        for (int n = 0; n < IM_ARRAYSIZE(items); n++)
        {
            const bool is_selected = (item_selected_idx == n);
            if (ImGui::Selectable(items[n], is_selected))
            {
                item_selected_idx = n;
                SetTextureFiltering(static_cast<TextureFiltering>(n));
            }

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}

void Renderer::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the D3D12 debug layer and GBV
    {
        ComPtr<ID3D12Debug> debugController0;
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController0))))
        {
            debugController0->EnableDebugLayer();
            if (SUCCEEDED(debugController0.As(&debugController1)))
            {
                debugController1->SetEnableGPUBasedValidation(TRUE);
            }
        }
    }

    // Use IDXGIInfoQueue
    ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
    if (SUCCEEDED(DXGIGetDebugInterface1(NULL, IID_PPV_ARGS(&dxgiInfoQueue))))
    {
        ThrowIfFailed(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE));
        ThrowIfFailed(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE));
        ThrowIfFailed(dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, TRUE));
    }

    // Set the DXGI factory debug flag
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    // Create factory
    ComPtr<IDXGIFactory4> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    // Create device using appropriate adapter
    ComPtr<ID3D12Device> device;
    if (m_useWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

        ThrowIfFailed(D3D12CreateDevice(
            warpAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)));
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        GetHardwareAdapter(factory.Get(), hardwareAdapter.GetAddressOf());

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&device)
        ));
    }
    ThrowIfFailed(device.As(&m_device));

#if defined(_DEBUG)
    // Use ID3D12InfoQueue
    // This querying is only successful when the debug layer is enabled.
    ComPtr<ID3D12InfoQueue> d3d12InfoQueue;
    if (SUCCEEDED(m_device.As(&d3d12InfoQueue)))
    {
        ThrowIfFailed(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
        ThrowIfFailed(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
        ThrowIfFailed(d3d12InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE));
    }
#endif

    // Check Enhanced barriers support
    D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {};
    ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12)));
    if (!options12.EnhancedBarriersSupported)
    {
        throw std::runtime_error("Enhanced Barriers are not supported on this hardware.");
    }

    // Do not transfer prvalue object to std::make_unique.
    // These are non-copyable and non-movable types.
    // Passing a temporary object like `std::make_unique<T>(T(...))` will fail to compile
    // Instead, pass the constructor arguments directly to std::make_unique<T>()
    m_commandQueue = std::make_unique<CommandQueue>(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    m_dynamicDescriptorHeap = std::make_unique<DynamicDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_uploadBuffer = std::make_unique<UploadBuffer>(m_device, 16 * 1024 * 1024);    // 16MB
    m_layoutTracker = std::make_unique<ResourceLayoutTracker>(m_device);
    for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        D3D12_DESCRIPTOR_HEAP_TYPE type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i);
        m_descriptorAllocators[i] = std::make_unique<DescriptorAllocator>(m_device, type);
        m_descriptorAllocators[i]->SetCommandQueue(m_commandQueue.get());       // Dependency injection
    }

    // Dependency injections
    m_commandQueue->SetDynamicDescriptorHeap(m_dynamicDescriptorHeap.get());
    m_dynamicDescriptorHeap->SetCommandQueue(m_commandQueue.get());
    m_uploadBuffer->SetCommandQueue(m_commandQueue.get());

    // For ImGui
    m_imguiDescriptorAllocator = std::make_unique<ImGuiDescriptorAllocator>(m_device.Get());

    // Check for Variable Refresh Rate(VRR)
    m_tearingSupported = CheckTearingSupport();

    // Describe and create the swap chain.
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = m_width;
    swapChainDesc.Height = m_height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = { 1, 0 };
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = FrameCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    ComPtr<IDXGISwapChain1> swapChain;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        m_commandQueue->GetCommandQueue().Get(),
        Win32Application::GetHwnd(),
        &swapChainDesc,
        nullptr,
        nullptr,
        swapChain.GetAddressOf()
    ));

    // GetCurrentBackBufferIndex을 사용하기 위해 IDXGISwapChain3로 쿼리
    ThrowIfFailed(swapChain.As(&m_swapChain));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Disable switching to fullscreen execlusive mode using ALT + ENTER
    ThrowIfFailed(factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

    // Create frame resources : RTV and command allocator for each frame
    auto alloc = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate(FrameCount);
    auto rtvAllocations = alloc.Split();
    for (UINT i = 0; i < FrameCount; i++)
    {
        auto frameResource = std::make_unique<FrameResource>(m_device.Get(), m_swapChain.Get(), i, std::move(rtvAllocations[i]));

        // Register backbuffer to tracker
        // Initial layout of backbuffer is D3D12_BARRIER_LAYOUT_COMMON : https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#initial-resource-state
        // depthOrArraySize and mipLevels for backbuffers are 1
        ID3D12Resource* pBackBuffer = frameResource->m_renderTarget.Get();
        auto desc = pBackBuffer->GetDesc();
        m_layoutTracker->RegisterResource(pBackBuffer, D3D12_BARRIER_LAYOUT_COMMON, desc.DepthOrArraySize, desc.MipLevels, DXGI_FORMAT_R8G8B8A8_UNORM);

        m_frameResources.push_back(std::move(frameResource));
    }
}

// Load the sample assets.
void Renderer::LoadAssets()
{
    // Compile shaders
    {
        UINT compileFlags = D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES;
#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        std::wstring vsName = L"vs.hlsl";
        std::wstring psName0 = L"ps.hlsl";
        std::wstring psName1 = L"PointLightShadowPS.hlsl";

        // conditional compilation for instancing
        std::vector<std::string> definesInstanced = { "INSTANCED" };
        std::vector<std::string> definesDepthOnly = { "DEPTH_ONLY" };
        std::vector<std::string> definesInstancedDepthOnly = { "INSTANCED", "DEPTH_ONLY" };

        std::vector<ShaderKey> shaderKeys;
        shaderKeys.push_back({ vsName, {}, "vs_5_0" });
        shaderKeys.push_back({ vsName, definesInstanced, "vs_5_0" });
        shaderKeys.push_back({ vsName, definesDepthOnly, "vs_5_0" });
        shaderKeys.push_back({ vsName, definesInstancedDepthOnly, "vs_5_0" });
        shaderKeys.push_back({ psName0, {}, "ps_5_1" });
        shaderKeys.push_back({ psName1, {}, "ps_5_1" });

        for (const ShaderKey& key : shaderKeys)
        {
            auto it = m_shaderBlobs.emplace(key, nullptr).first;
            std::vector<D3D_SHADER_MACRO> shaderMacros;
            for (const std::string& define : key.defines)
            {
                shaderMacros.push_back({ define.c_str(), NULL });
            }
            shaderMacros.push_back({ NULL, NULL });
            ThrowIfFailed(D3DCompileFromFile(key.fileName.c_str(), shaderMacros.data(), D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", key.target.c_str(), compileFlags, 0, &it->second, nullptr));
        }
    }

    // Define input layouts
    {
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescsDefault =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescsInstanced =
        {
            // Slot 0 for per-vertex data
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

            // Slot 1 for instanced data
            { "INSTANCE_WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },

            { "INSTANCE_INVTRANSPOSE", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_INVTRANSPOSE", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_INVTRANSPOSE", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
            { "INSTANCE_INVTRANSPOSE", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        };

        m_inputLayouts.emplace(MeshType::DEFUALT, std::move(inputElementDescsDefault));
        m_inputLayouts.emplace(MeshType::INSTANCED, std::move(inputElementDescsInstanced));
    }

    // Create the depth stencil view
    m_dsvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate();
    CreateDepthStencilBuffer(m_device.Get(), m_width, m_height, m_depthStencilBuffer, m_dsvAllocation);

    // Set viewport and scissorRect for shadow mapping
    m_shadowMapViewport = { 0.0f, 0.0f, static_cast<float>(m_shadowMapResolution), static_cast<float>(m_shadowMapResolution), 0.0f, 1.0f };
    m_shadowMapScissorRect = { 0, 0, static_cast<LONG>(m_shadowMapResolution), static_cast<LONG>(m_shadowMapResolution) };

    // Get command allocator and list for loading assets
    auto [commandAllocator, commandList] = m_commandQueue->GetAvailableCommandList();

    // Create constant buffers for each frame
    for (UINT i = 0; i < FrameCount; i++)
    {
        FrameResource& frameResource = *m_frameResources[i];

        // Main Camera
        if (i == 0) m_mainCameraIndex = UINT(frameResource.m_cameraConstantBuffers.size());
        frameResource.m_cameraConstantBuffers.push_back(std::make_unique<CameraCB>(m_device.Get()));

        // Material
        frameResource.m_materialConstantBuffers.push_back(std::make_unique<MaterialCB>(m_device.Get()));

        // Shadow
        frameResource.m_shadowConstantBuffer = std::make_unique<ShadowCB>(m_device.Get());
    }

    // Add meshes
    m_meshes.emplace_back(m_device.Get(), commandList, *m_uploadBuffer, m_frameResources, GeometryGenerator::GenerateCube());
    m_meshes.emplace_back(m_device.Get(), commandList, *m_uploadBuffer, m_frameResources, GeometryGenerator::GenerateSphere());
    m_instancedMeshes.emplace_back(m_device.Get(), commandList, *m_uploadBuffer, m_frameResources, GeometryGenerator::GenerateCube(), GeometryGenerator::GenerateSampleInstanceData());

    // Set up lights
    auto light = std::make_unique<DirectionalLight>(
        m_device.Get(),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(MAX_CASCADES),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(),
        m_shadowMapResolution,
        *m_layoutTracker,
        m_frameResources,
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount));
    light->SetDirection(XMVectorSet(-1.0f, -1.0f, 1.0f, 0.0f));
    m_lights.push_back(std::move(light));

    auto pointLight = std::make_unique<PointLight>(
        m_device.Get(),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(POINT_LIGHT_ARRAY_SIZE),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(),
        m_shadowMapResolution,
        *m_layoutTracker,
        m_frameResources,
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate(POINT_LIGHT_ARRAY_SIZE));
    pointLight->SetPosition(XMVectorSet(0.0f, 0.0f, 3.0f, 1.0f));
    pointLight->SetRange(10.0f);
    m_lights.push_back(std::move(pointLight));

    // Allocate textures
    auto alloc = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(3);
    auto textureAllocations = alloc.Split();
    m_albedo = std::make_unique<Texture>(
        m_device.Get(),
        commandList,
        std::move(textureAllocations[0]),
        *m_uploadBuffer,
        *m_layoutTracker,
        L"Assets/Textures/PavingStones150_4K-PNG_Color.png",
        true,
        true,
        false,
        false);

    m_normalMap = std::make_unique<Texture>(
        m_device.Get(),
        commandList,
        std::move(textureAllocations[1]),
        *m_uploadBuffer,
        *m_layoutTracker,
        L"Assets/Textures/PavingStones150_4K-PNG_NormalDX.png",
        false,
        true,
        false,
        false);

    m_heightMap = std::make_unique<Texture>(
        m_device.Get(),
        commandList,
        std::move(textureAllocations[2]),
        *m_uploadBuffer,
        *m_layoutTracker,
        L"Assets/Textures/PavingStones150_4K-PNG_Displacement.png",
        false,
        true,
        false,
        false);

    // Execute commands for loading assets and store fence value
    m_frameResources[m_frameIndex]->m_fenceValue = m_commandQueue->ExecuteCommandLists(commandAllocator, commandList, *m_layoutTracker);

    // Wait until assets have been uploaded to the GPU
    WaitForGPU();
}

void Renderer::PopulateCommandList(CommandList& commandList)
{
    PIX_SCOPED_EVENT(commandList.GetCommandList().Get(), PIX_COLOR_DEFAULT, L"PopulateCommandList");

    FrameResource& frameResource = *m_frameResources[m_frameIndex];

    auto cmdList = commandList.GetCommandList();

    // Set and parse root signature
    auto pRootSignature = GetRootSignature(m_currentRSKey);
    cmdList->SetGraphicsRootSignature(pRootSignature->GetRootSignature().Get());
    m_dynamicDescriptorHeap->ParseRootSignature(*pRootSignature);       // TODO : parse root signature only when root signature changed?

    // Bind light CBVs
    UINT32 numLights = static_cast<UINT32>(m_lights.size());
    for (UINT32 i = 0; i < numLights; ++i)
        m_dynamicDescriptorHeap->StageDescriptors(4, i, 1, frameResource.m_lightConstantBuffers[m_lights[i]->GetLightConstantBufferIndex()]->GetAllocationRef());

    // Depth-only pass for shadow mapping
    {
        cmdList->RSSetViewports(1, &m_shadowMapViewport);
        cmdList->RSSetScissorRects(1, &m_shadowMapScissorRect);

        // Pre-query PSOs
        m_currentPSOKey.meshType = MeshType::DEFUALT;
        m_currentPSOKey.passType = PassType::DEPTH_ONLY;
        m_currentPSOKey.vsKey = { L"vs.hlsl", {"DEPTH_ONLY"}, "vs_5_0" };
        m_currentPSOKey.psKey = { L"", {}, "" };
        auto* shadowPSO = GetPipelineState(m_currentPSOKey);

        m_currentPSOKey.meshType = MeshType::INSTANCED;
        m_currentPSOKey.vsKey.defines = { "INSTANCED", "DEPTH_ONLY" };
        auto* shadowPSOInstanced = GetPipelineState(m_currentPSOKey);

        m_currentPSOKey.psKey = { L"PointLightShadowPS.hlsl", {}, "ps_5_1" };
        auto* pointShadowPSOInstanced = GetPipelineState(m_currentPSOKey);

        m_currentPSOKey.meshType = MeshType::DEFUALT;
        m_currentPSOKey.vsKey.defines = { "DEPTH_ONLY" };
        auto* pointShadowPSO = GetPipelineState(m_currentPSOKey);

        for (UINT i = 0; i < m_lights.size(); ++i)
        {
            auto& light = m_lights[i];
            bool isPointLight = light->GetType() == LightType::POINT;

            if (isPointLight)
            {
                commandList.Barrier(
                    static_cast<PointLight*>(light.get())->GetRenderTarget(),
                    D3D12_BARRIER_SYNC_NONE,
                    D3D12_BARRIER_SYNC_RENDER_TARGET,
                    D3D12_BARRIER_ACCESS_NO_ACCESS,
                    D3D12_BARRIER_ACCESS_RENDER_TARGET,
                    D3D12_BARRIER_LAYOUT_RENDER_TARGET);

                cmdList->SetGraphicsRoot32BitConstant(9, i, 0);
            }
            else
            {
                // Barrier to change layout of shadowMap to depth write
                commandList.Barrier(
                    light->GetDepthBuffer(),
                    D3D12_BARRIER_SYNC_NONE,
                    D3D12_BARRIER_SYNC_DEPTH_STENCIL,
                    D3D12_BARRIER_ACCESS_NO_ACCESS,
                    D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
                    D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);
            }

            // Render each entry of shadow map.
            UINT16 arraySize = light->GetArraySize();
            for (UINT i = 0; i < arraySize; ++i)
            {
                auto shadowMapDsvHandle = light->GetDSVDescriptorHandle(i);

                if (isPointLight)
                {
                    auto rtvHandle = static_cast<PointLight*>(light.get())->GetRTVDescriptorHandle(i);
                    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &shadowMapDsvHandle);

                    XMVECTORF32 clearColor;
                    clearColor.v = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
                    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
                }
                else
                {
                    cmdList->OMSetRenderTargets(0, nullptr, FALSE, &shadowMapDsvHandle);
                }

                cmdList->ClearDepthStencilView(shadowMapDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

                cmdList->SetPipelineState(isPointLight ? pointShadowPSO : shadowPSO);
                for (const auto& mesh : m_meshes)
                {
                    cmdList->SetGraphicsRootConstantBufferView(0, frameResource.m_meshConstantBuffers[mesh.m_meshConstantBufferIndex]->GetGPUVirtualAddress());
                    cmdList->SetGraphicsRootConstantBufferView(1, frameResource.m_cameraConstantBuffers[light->GetCameraConstantBufferIndex(i)]->GetGPUVirtualAddress());
                    m_dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(cmdList);

                    mesh.Render(cmdList);
                }

                cmdList->SetPipelineState(isPointLight ? pointShadowPSOInstanced : shadowPSOInstanced);
                for (const auto& mesh : m_instancedMeshes)
                {
                    cmdList->SetGraphicsRootConstantBufferView(0, frameResource.m_meshConstantBuffers[mesh.m_meshConstantBufferIndex]->GetGPUVirtualAddress());
                    cmdList->SetGraphicsRootConstantBufferView(1, frameResource.m_cameraConstantBuffers[light->GetCameraConstantBufferIndex(i)]->GetGPUVirtualAddress());
                    m_dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(cmdList);

                    mesh.Render(cmdList);
                }
            }

            if (isPointLight)
            {
                commandList.Barrier(
                    static_cast<PointLight*>(light.get())->GetRenderTarget(),
                    D3D12_BARRIER_SYNC_RENDER_TARGET,
                    D3D12_BARRIER_SYNC_PIXEL_SHADING,
                    D3D12_BARRIER_ACCESS_RENDER_TARGET,
                    D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
                    D3D12_BARRIER_LAYOUT_SHADER_RESOURCE);
            }
            else
            {
                // Barrier to change layout of shadowMap to SRV
                commandList.Barrier(
                    light->GetDepthBuffer(),
                    D3D12_BARRIER_SYNC_DEPTH_STENCIL,
                    D3D12_BARRIER_SYNC_PIXEL_SHADING,
                    D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
                    D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
                    D3D12_BARRIER_LAYOUT_SHADER_RESOURCE);
            }
        }
    }

    // Color pass
    {
        cmdList->RSSetViewports(1, &m_viewport);
        cmdList->RSSetScissorRects(1, &m_scissorRect);

        // Pre-query PSOs
        m_currentPSOKey.meshType = MeshType::DEFUALT;
        m_currentPSOKey.passType = PassType::DEFAULT;
        m_currentPSOKey.vsKey = { L"vs.hlsl", {}, "vs_5_0" };
        m_currentPSOKey.psKey = { L"ps.hlsl", {}, "ps_5_1" };
        auto* pso = GetPipelineState(m_currentPSOKey);

        m_currentPSOKey.meshType = MeshType::INSTANCED;
        m_currentPSOKey.vsKey.defines = { "INSTANCED" };
        auto* psoInstanced = GetPipelineState(m_currentPSOKey);

        commandList.Barrier(
            frameResource.m_renderTarget.Get(),
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_RENDER_TARGET,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_RENDER_TARGET,
            D3D12_BARRIER_LAYOUT_RENDER_TARGET);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = frameResource.m_rtvAllocation.GetDescriptorHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvAllocation.GetDescriptorHandle();
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        // Use linear color for gamma-correct rendering
        XMVECTORF32 clearColor;
        clearColor.v = XMColorSRGBToRGB(XMVectorSet(0.5f, 0.5f, 0.5f, 1.0f));
        cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

        UINT32 numLights = static_cast<UINT32>(m_lights.size());

        // Bind textures
        m_dynamicDescriptorHeap->StageDescriptors(5, 0, 1, m_albedo->GetAllocationRef());
        m_dynamicDescriptorHeap->StageDescriptors(5, 1, 1, m_normalMap->GetAllocationRef());
        m_dynamicDescriptorHeap->StageDescriptors(5, 2, 1, m_heightMap->GetAllocationRef());

        // Bind shadow SRVs
        for (const auto& light : m_lights)
        {
            LightType type = light->GetType();
            UINT idxInArray = light->GetIdxInArray();
            m_dynamicDescriptorHeap->StageDescriptors(6 + static_cast<UINT32>(type), idxInArray, 1, light->GetSRVAllocationRef());
        }

        cmdList->SetPipelineState(pso);
        for (const auto& mesh : m_meshes)
        {
            cmdList->SetGraphicsRootConstantBufferView(0, frameResource.m_meshConstantBuffers[mesh.m_meshConstantBufferIndex]->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(1, frameResource.m_cameraConstantBuffers[m_mainCameraIndex]->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(2, frameResource.m_materialConstantBuffers[mesh.m_materialConstantBufferIndex]->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(3, frameResource.m_shadowConstantBuffer->GetGPUVirtualAddress());

            m_dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(cmdList);

            mesh.Render(cmdList);
        }

        cmdList->SetPipelineState(psoInstanced);
        for (const auto& mesh : m_instancedMeshes)
        {
            cmdList->SetGraphicsRootConstantBufferView(0, frameResource.m_meshConstantBuffers[mesh.m_meshConstantBufferIndex]->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(1, frameResource.m_cameraConstantBuffers[m_mainCameraIndex]->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(2, frameResource.m_materialConstantBuffers[mesh.m_materialConstantBufferIndex]->GetGPUVirtualAddress());
            cmdList->SetGraphicsRootConstantBufferView(3, frameResource.m_shadowConstantBuffer->GetGPUVirtualAddress());

            m_dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(cmdList);

            mesh.Render(cmdList);
        }
    }
}

// Wait for pending GPU work to complete
void Renderer::WaitForGPU()
{
    m_commandQueue->Flush();
}

void Renderer::MoveToNextFrame()
{
    // Update frame index and wait for fence value
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    m_commandQueue->WaitForFenceValue(m_frameResources[m_frameIndex]->m_fenceValue);
}

// Setup Dear ImGui context
void Renderer::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls

    ImGui_ImplWin32_Init(Win32Application::GetHwnd());

    // Setup Platform/Renderer backends
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = m_device.Get();
    init_info.CommandQueue = m_commandQueue->GetCommandQueue().Get();
    init_info.NumFramesInFlight = FrameCount;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    init_info.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    init_info.SrvDescriptorHeap = m_imguiDescriptorAllocator->GetDescriptorHeap();
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return ImGuiSrvDescriptorAllocate(out_cpu_handle, out_gpu_handle); };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return ImGuiSrvDescriptorFree(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&init_info);
}

void Renderer::SetTextureFiltering(TextureFiltering filtering)
{
    m_currentRSKey.filtering = filtering;
    m_currentPSOKey.filtering = filtering;
}

void Renderer::SetTextureAddressingMode(TextureAddressingMode addressingMode)
{
    m_currentRSKey.addressingMode = addressingMode;
    m_currentPSOKey.addressingMode = addressingMode;
}

void Renderer::SetMeshType(MeshType meshType)
{
    m_currentPSOKey.meshType = meshType;
}

RootSignature* Renderer::GetRootSignature(const RSKey& rsKey)
{
    auto [it, inserted] = m_rootSignatures.try_emplace(rsKey, std::make_unique<RootSignature>(10, 3));
    RootSignature& rootSignature = *it->second;

    // Create root signature if cache not exists.
    if (inserted)
    {
        // Root descriptor for MeshCB, CameraCB, MaterialCB, and ShadowCB
        rootSignature[0].InitAsDescriptor(0, 0, D3D12_SHADER_VISIBILITY_ALL, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);        // Mesh
        rootSignature[1].InitAsDescriptor(1, 0, D3D12_SHADER_VISIBILITY_ALL, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);        // Camera
        rootSignature[2].InitAsDescriptor(2, 0, D3D12_SHADER_VISIBILITY_PIXEL, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);      // Material
        rootSignature[3].InitAsDescriptor(3, 0, D3D12_SHADER_VISIBILITY_PIXEL, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);      // Shadow

        // Descriptor table for LightConstantBuffers[]
        rootSignature[4].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
        rootSignature[4].InitAsRange(0, 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);

        // Descriptor table for textures (albedo, normal map, height map)
        // When capture in PIX, app crashes if flag set by D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC. Very weird... should I report this to Microsoft?
        // GPU jobs (UpdateSubresources) are already finished when recording command list. I don't know why DATA_STATIC flag fails.
        rootSignature[5].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
        rootSignature[5].InitAsRange(0, 0, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);

        // Descriptor table for shadowMaps[]
        // Directional
        rootSignature[6].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
        rootSignature[6].InitAsRange(0, 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
        // Point
        rootSignature[7].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
        rootSignature[7].InitAsRange(0, 0, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
        // Spot
        rootSignature[8].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
        rootSignature[8].InitAsRange(0, 0, 3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);

        // Root constant for PointLightShadowPS
        rootSignature[9].InitAsConstant(4, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

        // Static samplers
        rootSignature.InitStaticSampler(0, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL, rsKey.filtering, rsKey.addressingMode);
        rootSignature.InitStaticSampler(1, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL, TextureFiltering::BILINEAR, TextureAddressingMode::BORDER, D3D12_COMPARISON_FUNC_GREATER_EQUAL);
        rootSignature.InitStaticSampler(2, 0, 2, D3D12_SHADER_VISIBILITY_PIXEL, TextureFiltering::BILINEAR, TextureAddressingMode::BORDER, D3D12_COMPARISON_FUNC_LESS_EQUAL);

        rootSignature.Finalize(m_device.Get());
    }

    return &rootSignature;
}

ID3D12PipelineState* Renderer::GetPipelineState(const PSOKey& psoKey)
{
    auto [it, inserted] = m_pipelineStates.try_emplace(psoKey, nullptr);

    if (inserted)
    {
        // Rasterizer State
        D3D12_RASTERIZER_DESC rasterizerDesc = {};
        rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
        rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
        rasterizerDesc.FrontCounterClockwise = FALSE;

        if (psoKey.passType == PassType::DEPTH_ONLY)
        {
            rasterizerDesc.DepthBias = -5000;
            rasterizerDesc.DepthBiasClamp = -0.1f;
            rasterizerDesc.SlopeScaledDepthBias = -5.0f;
        }
        else
        {
            rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
            rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
            rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        }

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

        // Depth-stencil
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
        depthStencilDesc.DepthEnable = TRUE;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        depthStencilDesc.StencilEnable = FALSE;
        depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
        { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        depthStencilDesc.FrontFace = defaultStencilOp;
        depthStencilDesc.BackFace = defaultStencilOp;

        ID3DBlob* vsBlob = GetShaderBlob(psoKey.vsKey);
        ID3DBlob* psBlob = GetShaderBlob(psoKey.psKey);

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { m_inputLayouts[psoKey.meshType].data(), static_cast<UINT>(m_inputLayouts[psoKey.meshType].size()) };
        psoDesc.pRootSignature = GetRootSignature(m_currentRSKey)->GetRootSignature().Get();

        // Shader stages are selected by demand.
        // VS is essential for rasterization.
        // PS is optional. (e.g. Depth-only pass)
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        if (psBlob)
        {
            psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        }

        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        if (psoKey.passType == PassType::DEPTH_ONLY)
        {
            if (psoKey.psKey.fileName == L"PointLightShadowPS.hlsl")
            {
                psoDesc.NumRenderTargets = 1;
                psoDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
            }
            else
            {
                psoDesc.NumRenderTargets = 0;
            }
        }
        else
        {
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        }
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&it->second)));
    }

    return it->second.Get();
}

ID3DBlob* Renderer::GetShaderBlob(const ShaderKey& shaderKey)
{
    if (shaderKey.IsEmpty())
    {
        return nullptr;
    }

    auto it = m_shaderBlobs.find(shaderKey);

    if (it == m_shaderBlobs.end())
    {
        throw std::runtime_error("Shaders should be baked in initialization or first run.");
    }

    return it->second.Get();
}

void Renderer::PrintFPS()
{
    static UINT64 frameCounter = 0;
    static double elapsedSeconds = 0.0;
    static std::chrono::high_resolution_clock clock;
    static auto t0 = clock.now();

    frameCounter++;
    auto t1 = clock.now();
    auto deltaTime = t1 - t0;
    t0 = t1;

    elapsedSeconds += deltaTime.count() * 1e-9;
    if (elapsedSeconds > 1.0)
    {
        WCHAR buffer[500];
        auto fps = frameCounter / elapsedSeconds;
        auto frameTime = 1000.0 / fps;
        swprintf_s(buffer, L"FPS: %f, Frame Time: %fms\n", fps, frameTime);
        OutputDebugStringW(buffer);

        frameCounter = 0;
        elapsedSeconds = 0.0;
    }
}

void Renderer::HandleInput()
{
    if (m_inputManager.IsKeyPressed(VK_ESCAPE))
    {
        PostQuitMessage(0);
        return;
    }

    if (m_inputManager.IsKeyPressed(VK_F11))
    {
        ToggleFullScreen();
    }

    if (m_inputManager.IsKeyPressed('V'))
    {
        m_vSync = !m_vSync;
        WCHAR buffer[500];
        swprintf_s(buffer, L"m_vSync : %d\n", m_vSync);
        OutputDebugStringW(buffer);
    }

    if (m_inputManager.IsKeyDown('W')) m_camera.MoveForward(0.01f);
    if (m_inputManager.IsKeyDown('A')) m_camera.MoveRight(-0.01f);
    if (m_inputManager.IsKeyDown('S')) m_camera.MoveForward(-0.01f);
    if (m_inputManager.IsKeyDown('D')) m_camera.MoveRight(0.01f);
    if (m_inputManager.IsKeyDown('Q')) m_camera.MoveUp(-0.01f);
    if (m_inputManager.IsKeyDown('E')) m_camera.MoveUp(0.01f);

    XMINT2 mouseMove = m_inputManager.GetAndResetMouseMove();
    m_camera.Rotate(mouseMove);
}

void Renderer::PrepareConstantData()
{
    // Mesh
    //for (auto& mesh : m_meshes)
    //{
    //    XMMATRIX world = XMMatrixScaling(1000.0f, 0.5f, 1000.0f) * XMMatrixTranslation(0.0f, -5.0f, 0.0f);
    //    mesh.m_meshConstantData.SetTransform(world);
    //    mesh.m_meshConstantData.textureTileScale = 50.0f;
    //}

    XMMATRIX world = XMMatrixScaling(1000.0f, 0.5f, 1000.0f) * XMMatrixTranslation(0.0f, -5.0f, 0.0f);
    m_meshes[0].m_meshConstantData.SetTransform(world);
    m_meshes[0].m_meshConstantData.textureTileScale = 50.0f;

    XMMATRIX w = XMMatrixTranslation(0.0f, -3.5f, 0.0f);
    m_meshes[1].m_meshConstantData.SetTransform(w);

    for (auto& mesh : m_instancedMeshes)
    {
        XMMATRIX prevWorld = XMMatrixTranspose(XMLoadFloat4x4(&mesh.m_meshConstantData.world));
        XMMATRIX world = prevWorld * XMMatrixRotationRollPitchYaw(0.0f, 0.001f, 0.0f);
        world = XMMatrixIdentity();
        mesh.m_meshConstantData.SetTransform(world);
        mesh.m_meshConstantData.textureTileScale = 1.0f;
    }

    // Main Camera
    m_cameraConstantData.cameraPos = m_camera.GetPosition();
    m_cameraConstantData.SetView(m_camera.GetViewMatrix());
    m_cameraConstantData.SetProjection(m_camera.GetProjectionMatrix());

    // Material
    m_materialConstantData.SetAmbient(XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f));
    m_materialConstantData.SetSpecular(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    m_materialConstantData.shininess = 10.0f;
    m_materialConstantData.textureIndices[0] = 0;
    m_materialConstantData.textureIndices[1] = 1;
    m_materialConstantData.textureIndices[2] = 2;

    // Light
    XMVECTOR lightDir = m_lights[0]->GetDirection();
    XMMATRIX rot = XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), 0.001f);
    XMVECTOR rotated = XMVector3Transform(lightDir, rot);
    m_lights[0]->SetDirection(rotated);

    PrepareCSM();

    // Point light
    {
        XMVECTOR pos = m_lights[1]->GetPosition();

        // +X, -X, +Y, -Y, +Z, -Z
        static const XMVECTOR Directions[6] = {
            { 1.0f, 0.0f, 0.0f, 0.0f },
            { -1.0f, 0.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, -1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f, 0.0f }
        };
        static const XMVECTOR Ups[6] = {
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 0.0f, -1.0f, 0.0f },
            { 0.0f, 0.0f, 1.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f },
            { 0.0f, 1.0f, 0.0f, 0.0f }
        };

        // 90 degree
        XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, m_lights[1]->GetRange(), 0.1f);

        for (UINT i = 0; i < POINT_LIGHT_ARRAY_SIZE; ++i)
        {
            XMMATRIX view = XMMatrixLookToLH(pos, Directions[i], Ups[i]);
            m_lights[1]->SetViewProjection(view, projection, i);
        }
    }

    // Set idxInArray for each light.
    UINT numDirectionalLight = 0;
    UINT numPointLight = 0;
    UINT numSpotLight = 0;
    for (auto& light : m_lights)
    {
        LightType type = light->GetType();

        UINT& cnt = (type == LightType::DIRECTIONAL) ? numDirectionalLight :
            (type == LightType::POINT ? numPointLight : numSpotLight);

        light->SetIdxInArray(cnt);
        ++cnt;
    }
}

void Renderer::PrepareCSM()
{
    // Create bounding frustum of view frustum and transform to world space.
    // BoundingFrustum::CreateFromMatrix and BoundingFrustum::GetCorners are implicitly assume that 0.0 is near plane and 1.0 is far plane.
    // When using reverse-z, you need to handle this.
    BoundingFrustum boundingFrustum;
    BoundingFrustum::CreateFromMatrix(boundingFrustum, m_camera.GetProjectionMatrix());
    XMMATRIX inverseView = XMMatrixInverse(nullptr, m_camera.GetViewMatrix());
    boundingFrustum.Transform(boundingFrustum, inverseView);

    // Get 8 corners of view frustum.
    //     Far     Near
    //    0----1  4----5
    //    |    |  |    |
    //    |    |  |    |
    //    3----2  7----6
    XMFLOAT3 frustumCorners[8];
    boundingFrustum.GetCorners(frustumCorners);

    XMFLOAT3 cascadeCorners[4][8];
    std::vector<BoundingSphere> frustumBSs(4);

    // Practical Split Scheme
    // https://developer.nvidia.com/gpugems/gpugems3/part-ii-light-and-shadows/chapter-10-parallel-split-shadow-maps-programmable-gpus
    const float nearPlane = m_camera.GetNearPlane();
    const float farPlane = m_camera.GetFarPlane();
    const float lambda = 0.9f;

    float splitRatio[MAX_CASCADES];

    for (UINT i = 1; i <= MAX_CASCADES; ++i)
    {
        float ratio = static_cast<float>(i) / MAX_CASCADES;
        float logSplit = nearPlane * std::pow(farPlane / nearPlane, ratio);
        float uniSplit = nearPlane + (farPlane - nearPlane) * ratio;

        float dist = lambda * logSplit + (1.0f - lambda) * uniSplit;
        m_shadowConstantData.cascadeSplits[i - 1].x = dist;

        splitRatio[i - 1] = (m_shadowConstantData.cascadeSplits[i - 1].x - nearPlane) / (farPlane - nearPlane);
    }

    for (UINT i = 0; i < MAX_CASCADES; ++i)
    {
        // Create corners.
        for (UINT j = 0; j < 4; ++j)
        {
            // 4, 5, 6, 7 are near plane.
            // 0, 1, 2, 3 are far plane.
            XMVECTOR n = XMLoadFloat3(&frustumCorners[j + 4]);
            XMVECTOR f = XMLoadFloat3(&frustumCorners[j]);

            XMVECTOR s = XMVectorLerp(n, f, i == 0 ? 0.0f : splitRatio[i - 1]);
            XMVECTOR e = XMVectorLerp(n, f, splitRatio[i]);

            XMStoreFloat3(&cascadeCorners[i][j], s);
            XMStoreFloat3(&cascadeCorners[i][j + 4], e);
        }

        // Create bounding sphere.
        BoundingSphere::CreateFromPoints(frustumBSs[i], 8, cascadeCorners[i], sizeof(XMFLOAT3));

        XMVECTOR center = XMLoadFloat3(&frustumBSs[i].Center);
        float radius = frustumBSs[i].Radius;

        XMFLOAT3 temp = m_camera.GetPosition();
        XMVECTOR cameraPos = XMLoadFloat3(&temp);

        XMVECTOR viewOriginToCenter = center - cameraPos;

        // Calculate view/projection matrix fit to light frustum
        for (auto& light : m_lights)
        {
            LightType type = light->GetType();

            switch (type)
            {
            case LightType::DIRECTIONAL:
            {
                // Since light type is guaranteed by m_type variable,
                // Use static_cast for downcasting instead of dynamic_cast.
                auto pLight = static_cast<DirectionalLight*>(light.get());

                static XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

                XMVECTOR dir = pLight->GetDirection();

                // Orthogonal projection of (center - view origin) onto lightDir.
                // This represents where the view origin is located relative to the center on the light's Z-axis.
                float d = XMVectorGetX(XMVector3Dot(viewOriginToCenter, dir));

                XMMATRIX view = XMMatrixLookToLH(center, dir, up);
                // Near Plane : Set to (view origin - sceneRadius) in Light Space.
                //              This ensures all shadow casters within 'sceneRadius' behind the camera are captured.
                // Far Plane :  Set to 'radius' to cover the entire bounding sphere of the view frustum.
                XMMATRIX projection = XMMatrixOrthographicLH(2 * radius, 2 * radius, radius, -d - farPlane);

                // Apply texel-sized increments to eliminate shadow shimmering.
                XMVECTOR shadowOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
                shadowOrigin = XMVector4Transform(shadowOrigin, view * projection);
                shadowOrigin = XMVectorScale(shadowOrigin, 1.0f / XMVectorGetW(shadowOrigin));      // Perspective divide. Can be ommitted if it uses orthographic projection.
                // [-1, 1] -> [-resolution / 2, resolution / 2]
                shadowOrigin = XMVectorScale(shadowOrigin, m_shadowMapResolution * 0.5f);           // Scaling based on shadow map resolution. We only need to scale it. No need to offset.

                // Calculate diff and apply as translation matrix.
                XMVECTOR roundedOrigin = XMVectorRound(shadowOrigin);
                XMVECTOR diff = roundedOrigin - shadowOrigin;
                diff = XMVectorScale(diff, 2.0f / m_shadowMapResolution);                           // Since diff is texel scale, it should be transformed to NDC scale.
                XMMATRIX fix = XMMatrixTranslation(XMVectorGetX(diff), XMVectorGetY(diff), 0.0f);

                light->SetViewProjection(view, projection * fix, i);
                break;
            }
            case LightType::POINT:
            {

                break;
            }
            case LightType::SPOT:
            {

                break;
            }
            }
        }
    }
}

void Renderer::UpdateConstantBuffers(FrameResource& frameResource)
{
    for (auto& mesh : m_meshes)
        mesh.UpdateMeshConstantBuffer(frameResource);
    for (auto& mesh : m_instancedMeshes)
        mesh.UpdateMeshConstantBuffer(frameResource);

    UpdateCameraConstantBuffer(frameResource);
    UpdateMaterialConstantBuffer(frameResource);

    for (auto& light : m_lights)
    {
        UINT16 arraySize = light->GetArraySize();
        for (UINT i = 0; i < arraySize; ++i)
        {
            light->UpdateCameraConstantBuffer(frameResource, i);
        }
        light->UpdateLightConstantBuffer(frameResource);
    }

    UpdateShadowConstantBuffer(frameResource);
}

void Renderer::UpdateCameraConstantBuffer(FrameResource& frameResource)
{
    frameResource.m_cameraConstantBuffers[m_mainCameraIndex]->Update(&m_cameraConstantData);
}

// Use index 0 for now.
void Renderer::UpdateMaterialConstantBuffer(FrameResource& frameResource)
{
    frameResource.m_materialConstantBuffers[0]->Update(&m_materialConstantData);
}

void Renderer::UpdateShadowConstantBuffer(FrameResource& frameResource)
{
    frameResource.m_shadowConstantBuffer->Update(&m_shadowConstantData);
}