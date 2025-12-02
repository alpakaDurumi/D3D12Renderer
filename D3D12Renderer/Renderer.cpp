#include "pch.h"
#include "Renderer.h"

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
#include "FrameResource.h"
#include "CommandList.h"

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
    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    LoadPipeline();
    LoadAssets();
    InitImGui();
}

void Renderer::OnUpdate()
{
    // Calculate FPS
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

    // 이번에 드로우할 프레임에 대해 constant buffers 업데이트
    FrameResource* pFrameResource = m_frameResources[m_frameIndex];

    m_materialConstantData.materialAmbient = { 0.1f, 0.1f, 0.1f };
    m_materialConstantData.materialSpecular = { 1.0f, 1.0f, 1.0f };
    m_materialConstantData.shininess = 10.0f;
    // For now, just use index 0
    pFrameResource->m_materialConstantBuffers[0]->Update(&m_materialConstantData);

    for (auto& mesh : m_meshes)
    {
        XMMATRIX world = XMMatrixScaling(1000.0f, 0.5f, 1000.0f) * XMMatrixTranslation(0.0f, -5.0f, 0.0f);
        XMStoreFloat4x4(&mesh.m_meshBufferData.world, XMMatrixTranspose(world));
        world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMStoreFloat4x4(&mesh.m_meshBufferData.inverseTranspose, XMMatrixInverse(nullptr, world));
        mesh.m_meshBufferData.textureTileScale = 50.0f;
        pFrameResource->m_meshConstantBuffers[mesh.m_meshConstantBufferIndex]->Update(&mesh.m_meshBufferData);
    }

    for (auto& mesh : m_instancedMeshes)
    {
        XMMATRIX prevWorld = XMMatrixTranspose(XMLoadFloat4x4(&mesh.m_meshBufferData.world));
        XMMATRIX world = prevWorld * XMMatrixRotationRollPitchYaw(0.0f, 0.001f, 0.0f);
        XMStoreFloat4x4(&mesh.m_meshBufferData.world, XMMatrixTranspose(world));
        world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMStoreFloat4x4(&mesh.m_meshBufferData.inverseTranspose, XMMatrixInverse(nullptr, world));
        mesh.m_meshBufferData.textureTileScale = 1.0f;
        pFrameResource->m_meshConstantBuffers[mesh.m_meshConstantBufferIndex]->Update(&mesh.m_meshBufferData);
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
    auto [commandAllocator, commandList] = m_commandQueue->GetAvailableCommandList();

    // Record all the commands we need to render the scene into the command list
    PopulateCommandList(commandList);

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
        D3D12_BARRIER_LAYOUT_PRESENT,
        { 0xffffffff, 0, 0, 0, 0, 0 });     // Select all subresources

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

    for (auto* pFrameResource : m_frameResources)
        delete pFrameResource;

    m_dsvAllocation.reset();
    m_texture.reset();

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
        DescriptorAllocation alloc = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate();

        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_frameResources[i]->m_renderTarget)));
        m_device->CreateRenderTargetView(m_frameResources[i]->m_renderTarget.Get(), nullptr, alloc.GetDescriptorHandle());
        m_frameResources[i]->m_rtvAllocation = std::move(alloc);

        // Assume that ResizeBuffers do not preserve previous layout.
        // For now, just use D3D12_BARRIER_LAYOUT_COMMON.
        auto desc = m_frameResources[i]->m_renderTarget->GetDesc();
        m_layoutTracker->RegisterResource(m_frameResources[i]->m_renderTarget.Get(), D3D12_BARRIER_LAYOUT_COMMON, desc.DepthOrArraySize, desc.MipLevels, DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    // Recreate DSV
    m_dsvAllocation = std::make_unique<DescriptorAllocation>(m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate());
    CreateDepthStencilBuffer(m_device.Get(), m_width, m_height, m_depthStencilBuffer, m_dsvAllocation->GetDescriptorHandle());
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
    for (UINT i = 0; i < FrameCount; i++)
    {
        DescriptorAllocation alloc = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate();

        FrameResource* pFrameResource = new FrameResource(m_device.Get(), m_swapChain.Get(), i, std::move(alloc));
        m_frameResources.push_back(pFrameResource);

        // Register backbuffer to tracker
        // Initial layout of backbuffer is D3D12_BARRIER_LAYOUT_COMMON : https://microsoft.github.io/DirectX-Specs/d3d/D3D12EnhancedBarriers.html#initial-resource-state
        // depthOrArraySize and mipLevels for backbuffers are 1
        ID3D12Resource* pBackBuffer = pFrameResource->m_renderTarget.Get();
        auto desc = pBackBuffer->GetDesc();
        m_layoutTracker->RegisterResource(pBackBuffer, D3D12_BARRIER_LAYOUT_COMMON, desc.DepthOrArraySize, desc.MipLevels, DXGI_FORMAT_R8G8B8A8_UNORM);
    }
}

// Load the sample assets.
void Renderer::LoadAssets()
{
    // Compile shaders
    {
        UINT compileFlags = 0;
#if defined(_DEBUG)
        // Enable better shader debugging with the graphics debugging tools.
        compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

        std::wstring vsName = L"vs.hlsl";
        std::wstring psName = L"ps.hlsl";

        // conditional compilation for instancing
        std::vector<std::string> definesInstanced = { "INSTANCED" };

        std::vector<ShaderKey> shaderKeys;
        shaderKeys.push_back({ vsName, std::vector<std::string>(), "vs_5_0" });
        shaderKeys.push_back({ vsName, definesInstanced, "vs_5_0" });
        shaderKeys.push_back({ psName, std::vector<std::string>(), "ps_5_0" });

        for (const ShaderKey& key : shaderKeys)
        {
            auto it = m_shaderBlobs.emplace(key, nullptr).first;
            std::vector<D3D_SHADER_MACRO> shaderMacros;
            for (const std::string& define : key.defines)
            {
                shaderMacros.push_back({ define.c_str(), NULL });
            }
            shaderMacros.push_back({ NULL, NULL });
            ThrowIfFailed(D3DCompileFromFile(key.fileName.c_str(), shaderMacros.data(), nullptr, "main", key.target.c_str(), compileFlags, 0, &it->second, nullptr));
        }
    }

    // Define input layouts
    {
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescsDefault =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };

        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescsInstanced =
        {
            // Slot 0 for per-vertex data
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
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
    m_dsvAllocation = std::make_unique<DescriptorAllocation>(m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate());
    CreateDepthStencilBuffer(m_device.Get(), m_width, m_height, m_depthStencilBuffer, m_dsvAllocation->GetDescriptorHandle());

    // Get command allocator and list for loading assets
    auto [commandAllocator, commandList] = m_commandQueue->GetAvailableCommandList();

    m_meshes.push_back(Mesh::MakeCube(m_device.Get(), commandList, *m_uploadBuffer));
    m_instancedMeshes.push_back(InstancedMesh::MakeCubeInstanced(m_device.Get(), commandList, *m_uploadBuffer));

    // Create constant buffers for each frame
    for (UINT i = 0; i < FrameCount; i++)
    {
        FrameResource* pFrameResource = m_frameResources[i];

        DescriptorAllocation alloc = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate();
        MaterialCB* matCB = new MaterialCB(m_device.Get(), std::move(alloc));
        pFrameResource->m_materialConstantBuffers.push_back(matCB);

        // Meshes
        {
            for (auto& mesh : m_meshes)
            {
                // Mesh
                {
                    MeshCB* meshCB = new MeshCB(m_device.Get());

                    // 각 FrameResource에서 동일한 인덱스긴 하지만, 한 번만 수행하도록 하였음
                    if (i == 0)
                        mesh.m_meshConstantBufferIndex = UINT(pFrameResource->m_meshConstantBuffers.size());
                    pFrameResource->m_meshConstantBuffers.push_back(meshCB);
                }

                if (i == 0)
                    mesh.m_materialConstantBufferIndex = 0;
            }

            for (auto& mesh : m_instancedMeshes)
            {
                // Mesh
                {
                    MeshCB* meshCB = new MeshCB(m_device.Get());

                    if (i == 0)
                        mesh.m_meshConstantBufferIndex = UINT(pFrameResource->m_meshConstantBuffers.size());
                    pFrameResource->m_meshConstantBuffers.push_back(meshCB);
                }

                if (i == 0)
                    mesh.m_materialConstantBufferIndex = 0;
            }
        }

        // Lights
        {
            DescriptorAllocation alloc = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate();
            LightCB* lightCB = new LightCB(m_device.Get(), std::move(alloc));
            pFrameResource->m_lightConstantBuffer = lightCB;
        }

        // Camera
        {
            DescriptorAllocation alloc = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate();
            CameraCB* cameraCB = new CameraCB(m_device.Get(), std::move(alloc));
            pFrameResource->m_cameraConstantBuffer = cameraCB;
        }
    }

    m_texture = std::make_unique<Texture>(
        m_device.Get(),
        commandList,
        *m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV],
        *m_uploadBuffer,
        *m_layoutTracker,
        L"Assets/Textures/PavingStones150_4K-PNG_Color.png",
        true,
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
    FrameResource* pFrameResource = m_frameResources[m_frameIndex];

    auto cmdList = commandList.GetCommandList();

    // Set and parse root signature
    auto pRootSignature = GetRootSignature(m_currentRSKey);
    cmdList->SetGraphicsRootSignature(pRootSignature->GetRootSignature().Get());
    m_dynamicDescriptorHeap->ParseRootSignature(*pRootSignature);       // TODO : parse root signature only when root signature changed?

    cmdList->RSSetViewports(1, &m_viewport);
    cmdList->RSSetScissorRects(1, &m_scissorRect);

    commandList.Barrier(
        pFrameResource->m_renderTarget.Get(),
        D3D12_BARRIER_SYNC_NONE,
        D3D12_BARRIER_SYNC_RENDER_TARGET,
        D3D12_BARRIER_ACCESS_NO_ACCESS,
        D3D12_BARRIER_ACCESS_RENDER_TARGET,
        D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        { 0xffffffff, 0, 0, 0, 0, 0 });     // Select all subresources

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = pFrameResource->m_rtvAllocation.GetDescriptorHandle();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvAllocation->GetDescriptorHandle();
    cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

    const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
    cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    m_currentPSOKey.vsKey = { L"vs.hlsl", {}, "vs_5_0" };
    m_currentPSOKey.psKey = { L"ps.hlsl", {}, "ps_5_0" };
    cmdList->SetPipelineState(GetPipelineState(m_currentPSOKey));
    for (const auto& mesh : m_meshes)
    {
        cmdList->SetGraphicsRootConstantBufferView(0, pFrameResource->m_meshConstantBuffers[mesh.m_meshConstantBufferIndex]->GetGPUVirtualAddress());
        cmdList->SetGraphicsRootConstantBufferView(1, pFrameResource->m_materialConstantBuffers[mesh.m_materialConstantBufferIndex]->GetGPUVirtualAddress());
        m_dynamicDescriptorHeap->StageDescriptors(2, 0, 1, pFrameResource->m_lightConstantBuffer->GetDescriptorHandle());
        m_dynamicDescriptorHeap->StageDescriptors(2, 1, 1, pFrameResource->m_cameraConstantBuffer->GetDescriptorHandle());
        m_dynamicDescriptorHeap->StageDescriptors(3, 0, 1, m_texture->GetDescriptorHandle());
        m_dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(cmdList);

        mesh.Render(cmdList);
    }

    m_currentPSOKey.vsKey.defines = { "INSTANCED" };
    SetMeshType(MeshType::INSTANCED);
    cmdList->SetPipelineState(GetPipelineState(m_currentPSOKey));
    for (const auto& mesh : m_instancedMeshes)
    {
        cmdList->SetGraphicsRootConstantBufferView(0, pFrameResource->m_meshConstantBuffers[mesh.m_meshConstantBufferIndex]->GetGPUVirtualAddress());
        cmdList->SetGraphicsRootConstantBufferView(1, pFrameResource->m_materialConstantBuffers[mesh.m_materialConstantBufferIndex]->GetGPUVirtualAddress());
        m_dynamicDescriptorHeap->StageDescriptors(2, 0, 1, pFrameResource->m_lightConstantBuffer->GetDescriptorHandle());
        m_dynamicDescriptorHeap->StageDescriptors(2, 1, 1, pFrameResource->m_cameraConstantBuffer->GetDescriptorHandle());
        m_dynamicDescriptorHeap->StageDescriptors(3, 0, 1, m_texture->GetDescriptorHandle());
        m_dynamicDescriptorHeap->CommitStagedDescriptorsForDraw(cmdList);

        mesh.Render(cmdList);
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
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
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
    auto [it, inserted] = m_rootSignatures.try_emplace(rsKey, std::make_unique<RootSignature>(4, 1));
    RootSignature& rootSignature = *it->second;

    // Create root signature if cache not exists.
    if (inserted)
    {
        // Root descriptor for MeshCB and MaterialCB
        rootSignature[0].InitAsDescriptor(0, 0, D3D12_SHADER_VISIBILITY_ALL, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);    // Mesh
        rootSignature[1].InitAsDescriptor(1, 0, D3D12_SHADER_VISIBILITY_PIXEL, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);     // Material

        // Descriptor table for LightCB and CameraCB
        rootSignature[2].InitAsTable(2, D3D12_SHADER_VISIBILITY_ALL);
        rootSignature[2].InitAsRange(0, 2, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);    // Light
        rootSignature[2].InitAsRange(1, 3, 0, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);    // Camera

        // Descriptor table for texture
        // When capture in PIX, app crashes if flag set by D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC. Very weird... should I report this to Microsoft?
        // In Resource history of PIX, only read occurs to this texture. So it seems like a bug of PIX.
        rootSignature[3].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
        rootSignature[3].InitAsRange(0, 0, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);

        rootSignature.InitStaticSampler(0, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL, rsKey.filtering, rsKey.addressingMode);

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

        // Depth-stencil
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

        // idx 0 : VS
        // idx 1 : PS
        ID3DBlob* vsBlob = GetShaderBlob(psoKey.vsKey);
        ID3DBlob* psBlob = GetShaderBlob(psoKey.psKey);

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.InputLayout = { m_inputLayouts[psoKey.meshType].data(), static_cast<UINT>(m_inputLayouts[psoKey.meshType].size()) };
        psoDesc.pRootSignature = GetRootSignature(m_currentRSKey)->GetRootSignature().Get();
        psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
        psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.SampleDesc.Count = 1;
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&it->second)));
    }

    return it->second.Get();
}

ID3DBlob* Renderer::GetShaderBlob(const ShaderKey& shaderKey)
{
    auto it = m_shaderBlobs.find(shaderKey);

    if (it == m_shaderBlobs.end())
    {
        throw std::runtime_error("Shaders should be baked in initialization or first run.");
    }

    return it->second.Get();
}