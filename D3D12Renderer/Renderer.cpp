#include "pch.h"
#include "Renderer.h"

#include <dxgidebug.h>
#include <filesystem>
#include <shlobj.h>
#include <thread>
#include <fstream>

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include "Win32Application.h"
#include "D3DHelper.h"
#include "SharedConfig.h"
#include "GeometryGenerator.h"

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

void Renderer::OnInit(UINT dpi)
{
    ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));   // For initializing DirectXTex
    LoadPipeline();
    LoadAssets();

    m_dpiScale = static_cast<float>(dpi) / USER_DEFAULT_SCREEN_DPI;

    InitImGui();
    m_prevTime = m_clock.now();
    m_deadLine = m_prevTime;
}

void Renderer::OnUpdate()
{
    auto now = m_clock.now();

    if (!m_vSync && m_fpsCap > 0)
    {
        auto targetMs = std::chrono::duration<double, std::milli>(1000.0 / m_fpsCap);

        if (now < m_deadLine)
        {
            std::this_thread::sleep_until(m_deadLine);
        }

        m_deadLine += std::chrono::duration_cast<std::chrono::steady_clock::duration>(targetMs);

        now = m_clock.now();
        // If too late, re-sync
        if (m_deadLine < now && (now - m_deadLine) > 2 * targetMs)
        {
            m_deadLine = now + std::chrono::duration_cast<std::chrono::steady_clock::duration>(targetMs);
        }
    }

    m_deltaTime = now - m_prevTime;
    m_prevTime = now;

    static double fixedDtMs = 1000.0 / 60.0;     // Target to 60Hz fixed time step
    static double accumulatedMs = 0.0;

    accumulatedMs += std::chrono::duration<double, std::milli>(m_deltaTime).count();
    while (accumulatedMs >= fixedDtMs)
    {
        FixedUpdate(fixedDtMs);
        accumulatedMs -= fixedDtMs;
    }

    float alpha = std::clamp(static_cast<float>(accumulatedMs / fixedDtMs), 0.0f, 1.0f);

    // 이번에 드로우할 프레임에 대해 constant buffers 업데이트
    FrameResource& frameResource = *m_frameResources[m_frameIndex];

    PrepareConstantData(alpha);
    UpdateConstantBuffers(frameResource);
}

// Render the scene.
void Renderer::OnRender()
{
    auto [commandAllocator, commandList] = m_commandQueue->GetAvailableCommandList();

    BuildImGuiFrame();
    PopulateCommandList(commandList);

    m_dynamicDescriptorHeapForCBVSRVUAV->Reset();

    // Populate commands for ImGui
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
        m_frameResources[m_frameIndex]->GetRenderTarget(),
        D3D12_BARRIER_SYNC_RENDER_TARGET,
        D3D12_BARRIER_SYNC_NONE,
        D3D12_BARRIER_ACCESS_RENDER_TARGET,
        D3D12_BARRIER_ACCESS_NO_ACCESS,
        D3D12_BARRIER_LAYOUT_PRESENT);

    // Execute the command lists and store the fence value
    // And notify fenceValue to UploadBuffer
    UINT64 fenceValue = m_commandQueue->ExecuteCommandLists(commandAllocator, commandList, *m_layoutTracker);
    m_frameResources[m_frameIndex]->SetFenceValue(fenceValue);
    m_uploadBuffer->QueueRetiredPages(fenceValue);
    m_dynamicDescriptorHeapForCBVSRVUAV->QueueRetiredHeaps(fenceValue);

    // Present the frame.
    UINT syncInterval = m_vSync ? 1 : 0;
    UINT presentFlags = m_tearingSupported && !m_vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;
    ThrowIfFailed(m_swapChain->Present(syncInterval, presentFlags));

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
        m_layoutTracker->UnregisterResource(m_frameResources[i]->GetRenderTarget());
        m_frameResources[i]->ResetRenderTarget();

        // Release previous gbuffers and create new ones
        for (UINT j = 0; j < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++j)
        {
            auto slot = static_cast<GBufferSlot>(j);
            m_layoutTracker->UnregisterResource(m_frameResources[i]->GetGBuffer(slot));
            m_frameResources[i]->ResetGBuffer(slot);
        }
        m_frameResources[i]->CreateGBuffers(width, height, *m_layoutTracker);

        m_frameResources[i]->SetFenceValue(m_frameResources[m_frameIndex]->GetFenceValue());
    }
    m_layoutTracker->UnregisterResource(m_depthStencilBuffer.Get());
    m_depthStencilBuffer.Reset();

    // Preserve existing format
    // Before calling ResizeBuffers, all backbuffer references should be released.
    ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, m_width, m_height, DXGI_FORMAT_UNKNOWN, m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate RTVs
    for (UINT i = 0; i < FrameCount; i++)
    {
        m_frameResources[i]->AcquireBackBuffer(m_swapChain.Get(), i);

        // Assume that ResizeBuffers do not preserve previous layout.
        // For now, just use D3D12_BARRIER_LAYOUT_COMMON.
        m_layoutTracker->RegisterResource(m_frameResources[i]->GetRenderTarget(), D3D12_BARRIER_LAYOUT_COMMON, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

        CreateRTV(m_device.Get(), m_frameResources[i]->GetRenderTarget(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, m_frameResources[i]->GetRTVHandle());
    }

    // Recreate depth-stencil buffer, DSV, and SRV
    CreateDepthStencilBuffer(m_device.Get(), m_width, m_height, 1, m_depthStencilBuffer, true);
    m_layoutTracker->RegisterResource(m_depthStencilBuffer.Get(), D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, 1, 1, DXGI_FORMAT_R24G8_TYPELESS);

    CreateDSV(m_device.Get(), m_depthStencilBuffer.Get(), m_dsvAllocation.GetDescriptorHandle(), true, false);
    CreateDSV(m_device.Get(), m_depthStencilBuffer.Get(), m_readOnlyDSVAllocation.GetDescriptorHandle(), true, true);
    CreateSRV(m_device.Get(), m_depthStencilBuffer.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, m_depthSRVAllocation.GetDescriptorHandle());
}

void Renderer::OnDpiChanged(UINT dpi)
{
    m_dpiScale = static_cast<float>(dpi) / USER_DEFAULT_SCREEN_DPI;
    ImGui::GetStyle().FontScaleMain = m_dpiScale;
}

void Renderer::BuildImGuiFrame()
{
    //ImGui::ShowDemoWindow(); // Show demo window! :)

    ImGui::Begin("Test");

    static UINT64 frameCounter = 0;
    static double elapsedSeconds = 0.0;

    ++frameCounter;

    static double fps = 0.0;
    static double frameTime = 0.0;

    elapsedSeconds += std::chrono::duration<double>(m_deltaTime).count();
    if (elapsedSeconds >= 1.0)
    {
        fps = frameCounter / elapsedSeconds;
        frameTime = 1000.0 / fps;

        frameCounter = 0;
        elapsedSeconds = 0.0;
    }

    ImGui::Text("FPS: %.1f", fps);
    ImGui::Text("Latency: %.3f", frameTime);

    ImGui::Checkbox("vSync", &m_vSync);

    const char* items0[] = { "Unlimited", "30", "60", "120", "144", "160", "240" };
    static int item0_selected_idx = 0;

    // FPS cap can be set when vSync enabled.
    ImGui::BeginDisabled(m_vSync);
    if (ImGui::BeginCombo("FPS Cap", items0[item0_selected_idx]))
    {
        for (int n = 0; n < IM_ARRAYSIZE(items0); ++n)
        {
            const bool is_selected = item0_selected_idx == n;
            if (ImGui::Selectable(items0[n], is_selected))
            {
                item0_selected_idx = n;
                SetFpsCap(std::string(items0[n]));
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();

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
    m_dynamicDescriptorHeapForCBVSRVUAV = std::make_unique<DynamicDescriptorHeap>(m_device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    m_uploadBuffer = std::make_unique<UploadBuffer>(m_device, 16 * 1024 * 1024);    // 16MB
    m_layoutTracker = std::make_unique<ResourceLayoutTracker>(m_device);
    for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        D3D12_DESCRIPTOR_HEAP_TYPE type = static_cast<D3D12_DESCRIPTOR_HEAP_TYPE>(i);
        m_descriptorAllocators[i] = std::make_unique<DescriptorAllocator>(m_device, type);
        m_descriptorAllocators[i]->SetCommandQueue(m_commandQueue.get());       // Dependency injection
    }

    // Create descriptor heap for samplers
    UINT numSamplers = static_cast<UINT>(TextureFiltering::NUM_TEXTURE_FILTERINGS) * static_cast<UINT>(TextureAddressingMode::NUM_TEXTURE_ADDRESSING_MODES);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
    heapDesc.NumDescriptors = numSamplers;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    ThrowIfFailed(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_samplerDescriptorHeap)));

    // Dependency injections
    m_commandQueue->SetDescriptorHeaps(m_dynamicDescriptorHeapForCBVSRVUAV.get(), m_samplerDescriptorHeap.Get());
    m_dynamicDescriptorHeapForCBVSRVUAV->SetCommandQueue(m_commandQueue.get());
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
        auto frameResource = std::make_unique<FrameResource>(m_device.Get(), m_swapChain.Get(), i,
            *m_layoutTracker,
            std::move(rtvAllocations[i]),
            m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate(static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS)),
            m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS)));

        m_frameResources.push_back(std::move(frameResource));
    }

    CreateRootSignature();
    m_dynamicDescriptorHeapForCBVSRVUAV->ParseRootSignature(*m_rootSignature);
}

// Load the sample assets.
void Renderer::LoadAssets()
{
    // Read shaders
    {
        std::vector<std::wstring> shaderNames;
        shaderNames.push_back(L"vs.cso");
        shaderNames.push_back(L"vs_depth_only.cso");
        shaderNames.push_back(L"ps.cso");
        shaderNames.push_back(L"PointLightShadowPS.cso");
        shaderNames.push_back(L"GBufferPS.cso");
        shaderNames.push_back(L"DeferredLightingVS.cso");
        shaderNames.push_back(L"DeferredLightingPS.cso");

        for (const auto& name : shaderNames)
        {
            std::ifstream file(std::filesystem::path(L"shaders") / name, std::ios::binary);
            if (!file)
            {
                throw std::runtime_error("Failed to open .cso file.");
            }

            m_shaderBlobs.try_emplace(ShaderKey{ name }, std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
        }
    }

    // Define input layout
    m_inputLayout =
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

        { "INSTANCE_MATERIAL_INDEX", 0, DXGI_FORMAT_R32_UINT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    // Create depth-stencil buffer, DSV, and SRV
    m_dsvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate();
    m_readOnlyDSVAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate();
    m_depthSRVAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate();

    CreateDepthStencilBuffer(m_device.Get(), m_width, m_height, 1, m_depthStencilBuffer, true);
    m_layoutTracker->RegisterResource(m_depthStencilBuffer.Get(), D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE, 1, 1, DXGI_FORMAT_R24G8_TYPELESS);

    CreateDSV(m_device.Get(), m_depthStencilBuffer.Get(), m_dsvAllocation.GetDescriptorHandle(), true, false);
    CreateDSV(m_device.Get(), m_depthStencilBuffer.Get(), m_readOnlyDSVAllocation.GetDescriptorHandle(), true, true);
    CreateSRV(m_device.Get(), m_depthStencilBuffer.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, m_depthSRVAllocation.GetDescriptorHandle());

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
        if (i == 0) m_mainCameraIndex = frameResource.GetCameraConstantBufferCount();
        frameResource.AddCameraConstantBuffer();

        // Shadow
        frameResource.CreateShadowConstantBuffer();
    }

    // Add samplers
    auto baseCPUHandle = m_samplerDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    auto incrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    for (UINT i = 0; i < static_cast<UINT>(TextureFiltering::NUM_TEXTURE_FILTERINGS); ++i)
    {
        for (UINT j = 0; j < static_cast<UINT>(TextureAddressingMode::NUM_TEXTURE_ADDRESSING_MODES); ++j)
        {
            UINT idx = i * static_cast<UINT>(TextureAddressingMode::NUM_TEXTURE_ADDRESSING_MODES) + j;
            auto cpuHandle = GetCPUDescriptorHandle(baseCPUHandle, idx, incrementSize);
            CreateSampler(m_device.Get(), static_cast<TextureFiltering>(i), static_cast<TextureAddressingMode>(j), cpuHandle);
        }
    }

    // Allocate textures
    auto alloc = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(3);
    auto textureAllocations = alloc.Split();

    CreateTexture(
        commandList,
        std::move(textureAllocations[0]),
        L"assets/textures/PavingStones150_4K-PNG_Color.png",
        true,
        true,
        false,
        false);

    CreateTexture(
        commandList,
        std::move(textureAllocations[1]),
        L"assets/textures/PavingStones150_4K-PNG_NormalDX.png",
        false,
        true,
        false,
        false);

    CreateTexture(
        commandList,
        std::move(textureAllocations[2]),
        L"assets/textures/PavingStones150_4K-PNG_Displacement.png",
        false,
        true,
        false,
        false);

    // Add materials
    auto* pBaseMat = CreateMaterial();
    pBaseMat->SetAmbient(XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f));
    pBaseMat->SetSpecular(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    pBaseMat->SetShininess(10.0f);
    pBaseMat->SetTextureIndices(0, 1, 2);
    pBaseMat->SetTextureAddressingModes(TextureAddressingMode::WRAP, TextureAddressingMode::WRAP, TextureAddressingMode::WRAP);
    pBaseMat->BuildSamplerIndices(m_currentTextureFiltering);
    pBaseMat->SetRenderingPath(RenderingPath::DEFERRED);

    auto* pPlaneMat = CloneMaterial(*pBaseMat);
    pPlaneMat->SetTextureTileScales(50.0f, 50.0f, 50.0f);

    // Add meshes
    auto cubeMesh = std::make_unique<Mesh>(m_device.Get(), commandList, *m_uploadBuffer, GeometryGenerator::GenerateCube());
    auto sphereMesh = std::make_unique<Mesh>(m_device.Get(), commandList, *m_uploadBuffer, GeometryGenerator::GenerateSphere());

    auto* pCubeMesh = cubeMesh.get();
    auto* pSphereMesh = sphereMesh.get();

    m_meshes.push_back(std::move(cubeMesh));
    m_meshes.push_back(std::move(sphereMesh));

    // Add RenderObjects
    auto* pPlane = CreateRenderObject(pCubeMesh, pPlaneMat);
    pPlane->SetInitialTransform(XMFLOAT3(1000.0f, 0.5f, 1000.0f), XMFLOAT3(), XMFLOAT3(0.0f, -5.0f, 0.0f));

    m_forwardRenderObjects[pCubeMesh].reserve(10001);
    m_deferredRenderObjects[pCubeMesh].reserve(10001);

    for (UINT i = 0; i < 100; i++)
    {
        for (UINT j = 0; j < 100; j++)
        {
            auto* pCube = CreateRenderObject(pCubeMesh, pBaseMat);
            pCube->SetInitialTransform(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(), XMFLOAT3((i - 50.0f) * 4.0f, (j - 50.0f) * 4.0f, 10.0f));
            m_previewRotations.push_back(pCube);
        }
    }

    auto* pSphere = CreateRenderObject(pSphereMesh, pBaseMat);
    pSphere->SetInitialTransform(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(), XMFLOAT3(0.0f, -3.5f, 0.0f));

    // Set up lights
    auto* pDirectionalLight = CreateLight<DirectionalLight>();
    pDirectionalLight->SetDirection(XMFLOAT3(-1.0f, -1.0f, 1.0f));

    auto* pPointLight = CreateLight<PointLight>();
    pPointLight->SetPosition(XMFLOAT3(0.0f, 4.0f, 3.0f));
    pPointLight->SetRange(30.0f);

    auto* pSpotLight = CreateLight<SpotLight>();
    pSpotLight->SetPosition(XMFLOAT3(0.0f, 10.0f, -5.0f));
    pSpotLight->SetDirection(XMFLOAT3(0.0f, -1.0f, 1.0f));
    pSpotLight->SetRange(50.0f);
    pSpotLight->SetAngles(50.0f, 10.0f);

    // Execute commands for loading assets and store fence value
    m_frameResources[m_frameIndex]->SetFenceValue(m_commandQueue->ExecuteCommandLists(commandAllocator, commandList, *m_layoutTracker));

    // Wait until assets have been uploaded to the GPU
    WaitForGPU();
}

void Renderer::PopulateCommandList(CommandList& commandList)
{
    PIX_SCOPED_EVENT(commandList.GetCommandList().Get(), PIX_COLOR_DEFAULT, L"PopulateCommandList");

    FrameResource& frameResource = *m_frameResources[m_frameIndex];
    frameResource.ResetInstanceOffsetByte();

    auto cmdList = commandList.GetCommandList();

    // Set root signature
    cmdList->SetGraphicsRootSignature(m_rootSignature->GetRootSignature().Get());

    UINT numLights = 0;
    for (const auto& vec : m_lights) numLights += static_cast<UINT>(vec.size());
    cmdList->SetGraphicsRoot32BitConstant(2, numLights, 0);

    // Stage material CBVs
    UINT numMaterials = static_cast<UINT>(m_materials.size());
    for (UINT i = 0; i < numMaterials; ++i)
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(4, i, 1, frameResource.GetMaterialCBVAllocationRef(i));

    // Stage light CBVs
    for (UINT i = 0; i < numLights; ++i)
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(5, i, 1, frameResource.GetLightCBVAllocationRef(i));

    // Stage textures
    UINT numTextures = static_cast<UINT>(m_textures.size());
    for (UINT i = 0; i < numTextures; ++i)
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(6, i, 1, m_textures[i]->GetAllocationRef());

    // Stage shadow SRVs
    for (UINT type = 0; type < static_cast<UINT>(LightType::NUM_LIGHT_TYPES); ++type)
    {
        for (auto& light : m_lights[type])
        {
            UINT idxInArray = light->GetIdxInArray();
            m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(7 + type, idxInArray, 1, light->GetSRVAllocationRef());
        }
    }

    m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(10, 0, static_cast<UINT32>(GBufferSlot::NUM_GBUFFER_SLOTS), frameResource.GetGBufferSRVAllocationRef());
    m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(10, static_cast<UINT32>(GBufferSlot::NUM_GBUFFER_SLOTS), 1, m_depthSRVAllocation);

    BindDescriptorTables(cmdList.Get());

    std::vector<InstanceData> temp;
    UINT curOffset = 0;
    for (auto& mesh : m_meshes)
    {
        auto* pMesh = mesh.get();

        for (auto& object : m_forwardRenderObjects[pMesh])
            temp.push_back(object.BuildInstanceData());
        for (auto& object : m_deferredRenderObjects[pMesh])
            temp.push_back(object.BuildInstanceData());

        m_instanceRanges[pMesh].offset = curOffset;
        m_instanceRanges[pMesh].forwardCount = static_cast<UINT>(m_forwardRenderObjects[pMesh].size());
        m_instanceRanges[pMesh].deferredCount = static_cast<UINT>(m_deferredRenderObjects[pMesh].size());
        curOffset += m_instanceRanges[pMesh].forwardCount + m_instanceRanges[pMesh].deferredCount;
    }

    frameResource.EnsureInstanceCapacity(static_cast<UINT>(temp.size()));
    frameResource.PushInstanceData(temp);

    // Depth-only pass for shadow mapping
    // Also work as z-prepass
    {
        PIX_SCOPED_EVENT(commandList.GetCommandList().Get(), PIX_COLOR_DEFAULT, L"Depth-only pass");

        cmdList->RSSetViewports(1, &m_shadowMapViewport);
        cmdList->RSSetScissorRects(1, &m_shadowMapScissorRect);

        // Pre-query PSOs
        m_currentPSOKey.passType = PassType::DEPTH_ONLY;
        m_currentPSOKey.vsName = L"vs.hlsl";
        m_currentPSOKey.psName = L"";
        auto* shadowPSO = GetPipelineState(m_currentPSOKey);

        m_currentPSOKey.psName = L"PointLightShadowPS.hlsl";
        auto* pointShadowPSO = GetPipelineState(m_currentPSOKey);

        UINT lightIdx = 0;
        for (UINT type = 0; type < static_cast<UINT>(LightType::NUM_LIGHT_TYPES); ++type)
        {
            bool isPointLight = static_cast<LightType>(type) == LightType::POINT;

            for (auto& light : m_lights[type])
            {
                if (isPointLight)
                {
                    commandList.Barrier(
                        static_cast<PointLight*>(light.get())->GetRenderTarget(),
                        D3D12_BARRIER_SYNC_NONE,
                        D3D12_BARRIER_SYNC_RENDER_TARGET,
                        D3D12_BARRIER_ACCESS_NO_ACCESS,
                        D3D12_BARRIER_ACCESS_RENDER_TARGET,
                        D3D12_BARRIER_LAYOUT_RENDER_TARGET);

                    cmdList->SetGraphicsRoot32BitConstant(3, lightIdx, 0);
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
                for (UINT j = 0; j < arraySize; ++j)
                {
                    auto shadowMapDsvHandle = light->GetDSVDescriptorHandle(j);

                    if (isPointLight)
                    {
                        auto rtvHandle = static_cast<PointLight*>(light.get())->GetRTVDescriptorHandle(j);
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

                    cmdList->SetGraphicsRootConstantBufferView(0, frameResource.GetCameraCBVirtualAddress(light->GetCameraConstantBufferBaseIndex() + j));

                    for (auto& mesh : m_meshes)
                    {
                        DrawMesh(cmdList.Get(), *mesh, PassType::DEPTH_ONLY, frameResource.GetInstanceBufferVirtualAddress());
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
                ++lightIdx;
            }
        }
    }

    // temp
    commandList.Barrier(
        m_depthStencilBuffer.Get(),
        D3D12_BARRIER_SYNC_NONE,
        D3D12_BARRIER_SYNC_DEPTH_STENCIL,
        D3D12_BARRIER_ACCESS_NO_ACCESS,
        D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
        D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);

    // GBuffer pass
    {
        PIX_SCOPED_EVENT(commandList.GetCommandList().Get(), PIX_COLOR_DEFAULT, L"GBuffer pass");

        cmdList->RSSetViewports(1, &m_viewport);
        cmdList->RSSetScissorRects(1, &m_scissorRect);

        // Pre-query PSOs
        m_currentPSOKey.passType = PassType::GBUFFER;
        m_currentPSOKey.vsName = L"vs.hlsl";
        m_currentPSOKey.psName = L"GBufferPS.hlsl";
        auto* pso = GetPipelineState(m_currentPSOKey);

        for (UINT i = 0; i < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++i)
        {
            commandList.Barrier(
                frameResource.GetGBuffer(static_cast<GBufferSlot>(i)),
                D3D12_BARRIER_SYNC_NONE,
                D3D12_BARRIER_SYNC_RENDER_TARGET,
                D3D12_BARRIER_ACCESS_NO_ACCESS,
                D3D12_BARRIER_ACCESS_RENDER_TARGET,
                D3D12_BARRIER_LAYOUT_RENDER_TARGET);
        }

        auto rtvHandles = frameResource.GetGBufferRTVHandles();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvAllocation.GetDescriptorHandle();
        cmdList->OMSetRenderTargets(static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS), rtvHandles.data(), FALSE, &dsvHandle);

        XMVECTORF32 clearColor;
        clearColor.v = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
        for (auto& rtvHandle : rtvHandles)
            cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
        cmdList->OMSetStencilRef(1);

        cmdList->SetPipelineState(pso);

        cmdList->SetGraphicsRootConstantBufferView(0, frameResource.GetCameraCBVirtualAddress(m_mainCameraIndex));

        for (auto& mesh : m_meshes)
        {
            DrawMesh(cmdList.Get(), *mesh, PassType::GBUFFER, frameResource.GetInstanceBufferVirtualAddress());
        }

        for (UINT i = 0; i < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++i)
        {
            commandList.Barrier(
                frameResource.GetGBuffer(static_cast<GBufferSlot>(i)),
                D3D12_BARRIER_SYNC_RENDER_TARGET,
                D3D12_BARRIER_SYNC_PIXEL_SHADING,
                D3D12_BARRIER_ACCESS_RENDER_TARGET,
                D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
                D3D12_BARRIER_LAYOUT_SHADER_RESOURCE);
        }
    }

    // To use depth-stencil buffer as both SRV and DSV simultaneously
    // SRV: to reconstruct world pos
    // DSV: for stencil test
    commandList.Barrier(
        m_depthStencilBuffer.Get(),
        D3D12_BARRIER_SYNC_DEPTH_STENCIL,
        D3D12_BARRIER_SYNC_PIXEL_SHADING,
        D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
        D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ,
        D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ);

    // Deferred Lighting pass
    {
        PIX_SCOPED_EVENT(commandList.GetCommandList().Get(), PIX_COLOR_DEFAULT, L"Deferred Lighting pass");

        cmdList->RSSetViewports(1, &m_viewport);
        cmdList->RSSetScissorRects(1, &m_scissorRect);

        // Pre-query PSOs
        m_currentPSOKey.passType = PassType::DEFERRED_LIGHTING;
        m_currentPSOKey.vsName = L"DeferredLightingVS.hlsl";
        m_currentPSOKey.psName = L"DeferredLightingPS.hlsl";
        auto* pso = GetPipelineState(m_currentPSOKey);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = frameResource.GetRTVHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_readOnlyDSVAllocation.GetDescriptorHandle();
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
        cmdList->OMSetStencilRef(1);

        // Use linear color for gamma-correct rendering
        XMVECTORF32 clearColor;
        clearColor.v = XMColorSRGBToRGB(XMVectorSet(0.5f, 0.5f, 0.5f, 1.0f));
        cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        cmdList->SetPipelineState(pso);

        cmdList->SetGraphicsRootConstantBufferView(0, frameResource.GetCameraCBVirtualAddress(m_mainCameraIndex));
        cmdList->SetGraphicsRootConstantBufferView(1, frameResource.GetShadowCBVirtualAddress());

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawInstanced(3, 1, 0, 0);
    }

    // restore
    commandList.Barrier(
        m_depthStencilBuffer.Get(),
        D3D12_BARRIER_SYNC_PIXEL_SHADING,
        D3D12_BARRIER_SYNC_DEPTH_STENCIL,
        D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ,
        D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
        D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE);

    // Forward Color pass
    {
        PIX_SCOPED_EVENT(commandList.GetCommandList().Get(), PIX_COLOR_DEFAULT, L"Forward color pass");

        cmdList->RSSetViewports(1, &m_viewport);
        cmdList->RSSetScissorRects(1, &m_scissorRect);

        // Pre-query PSOs
        m_currentPSOKey.passType = PassType::FORWARD_COLORING;
        m_currentPSOKey.vsName = L"vs.hlsl";
        m_currentPSOKey.psName = L"ps.hlsl";
        auto* pso = GetPipelineState(m_currentPSOKey);

        commandList.Barrier(
            frameResource.GetRenderTarget(),
            D3D12_BARRIER_SYNC_NONE,
            D3D12_BARRIER_SYNC_RENDER_TARGET,
            D3D12_BARRIER_ACCESS_NO_ACCESS,
            D3D12_BARRIER_ACCESS_RENDER_TARGET,
            D3D12_BARRIER_LAYOUT_RENDER_TARGET);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = frameResource.GetRTVHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvAllocation.GetDescriptorHandle();
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        cmdList->SetPipelineState(pso);

        cmdList->SetGraphicsRootConstantBufferView(0, frameResource.GetCameraCBVirtualAddress(m_mainCameraIndex));
        cmdList->SetGraphicsRootConstantBufferView(1, frameResource.GetShadowCBVirtualAddress());

        for (auto& mesh : m_meshes)
        {
            DrawMesh(cmdList.Get(), *mesh, PassType::FORWARD_COLORING, frameResource.GetInstanceBufferVirtualAddress());
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
    m_commandQueue->WaitForFenceValue(m_frameResources[m_frameIndex]->GetFenceValue());
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

    ImGui::GetStyle().FontScaleMain = m_dpiScale;
}

void Renderer::SetTextureFiltering(TextureFiltering filtering)
{
    m_currentTextureFiltering = filtering;
    for (auto& material : m_materials)
    {
        material->BuildSamplerIndices(m_currentTextureFiltering);
    }
}

Material* Renderer::CreateMaterial()
{
    auto material = std::make_unique<Material>(m_device.Get(), m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount), m_frameResources);
    auto* pMat = material.get();
    m_materials.push_back(std::move(material));
    return pMat;
}

Material* Renderer::CloneMaterial(const Material& src)
{
    auto pMat = CreateMaterial();
    pMat->CopyDataFrom(src);
    return pMat;
}

RenderObject* Renderer::CreateRenderObject(Mesh* pMesh, Material* mat)
{
    RenderingPath path = mat->GetRenderingPath();
    RenderObject* ret = nullptr;
    if (path == RenderingPath::FORWARD)
    {
        m_forwardRenderObjects[pMesh].emplace_back(pMesh);
        ret = &m_forwardRenderObjects[pMesh].back();
    }
    else if (path == RenderingPath::DEFERRED)
    {
        m_deferredRenderObjects[pMesh].emplace_back(pMesh);
        ret = &m_deferredRenderObjects[pMesh].back();
    }

    if (!ret) throw std::runtime_error("Unsupported Rendering Path.");

    ret->SetMaterial(mat);
    return ret;
}

Texture* Renderer::CreateTexture(
    CommandList& commandList,
    DescriptorAllocation&& allocation,
    const std::wstring& filePath,
    bool isSRGB,
    bool useBlockCompress,
    bool flipImage,
    bool isCubeMap)
{
    auto texture = std::make_unique<Texture>(
        m_device.Get(),
        commandList,
        std::move(allocation),
        *m_uploadBuffer,
        *m_layoutTracker,
        filePath,
        isSRGB,
        useBlockCompress,
        flipImage,
        isCubeMap);

    auto* pTexture = texture.get();
    m_textures.push_back(std::move(texture));

    return pTexture;
}

void Renderer::SetFpsCap(std::string fps)
{
    if (fps == "Unlimited")
    {
        m_fpsCap = -1;
    }
    else
    {
        m_fpsCap = std::stoi(fps);
        m_deadLine = m_clock.now();
    }
}

void Renderer::BindDescriptorTables(ID3D12GraphicsCommandList7* pCommandList)
{
    bool CBVSRVUAVHeapChanged = m_dynamicDescriptorHeapForCBVSRVUAV->CheckHeapChanged();

    if (CBVSRVUAVHeapChanged)
    {
        ID3D12DescriptorHeap* ppHeaps[] = { m_dynamicDescriptorHeapForCBVSRVUAV->GetCurrentDescriptorHeap(), m_samplerDescriptorHeap.Get() };
        pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    }

    m_dynamicDescriptorHeapForCBVSRVUAV->CommitStagedDescriptorsForDraw(pCommandList);
    pCommandList->SetGraphicsRootDescriptorTable(11, m_samplerDescriptorHeap->GetGPUDescriptorHandleForHeapStart());    // Root parameter 11
}

void Renderer::CreateRootSignature()
{
    m_rootSignature = std::make_unique<RootSignature>(12, 2);
    auto& rootSignature = *m_rootSignature;

    // Root descriptor for CameraCB and ShadowCB
    rootSignature[0].InitAsDescriptor(0, 0, D3D12_SHADER_VISIBILITY_ALL, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);        // Camera
    rootSignature[1].InitAsDescriptor(1, 0, D3D12_SHADER_VISIBILITY_PIXEL, D3D12_ROOT_PARAMETER_TYPE_CBV, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC);      // Shadow

    // Root constants for number of lights
    rootSignature[2].InitAsConstant(2, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // Root constant for PointLightShadowPS
    rootSignature[3].InitAsConstant(3, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);

    // Descriptor table for MaterialConstantBuffers[]
    rootSignature[4].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSignature[4].InitAsRange(0, 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

    // Descriptor table for LightConstantBuffers[]
    rootSignature[5].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSignature[5].InitAsRange(0, 0, 2, D3D12_DESCRIPTOR_RANGE_TYPE_CBV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

    // Descriptor table for textures (albedo, normal map, height map)
    rootSignature[6].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSignature[6].InitAsRange(0, 0, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

    // Descriptor table for shadowMaps[]
    // Directional
    rootSignature[7].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSignature[7].InitAsRange(0, 0, 1, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
    // Point
    rootSignature[8].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSignature[8].InitAsRange(0, 0, 2, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
    // Spot
    rootSignature[9].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSignature[9].InitAsRange(0, 0, 3, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, UINT_MAX, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

    // Descriptor table for GBuffers and SRV for depth stencil buffer
    rootSignature[10].InitAsTable(2, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSignature[10].InitAsRange(0, 0, 4, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS), D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);
    rootSignature[10].InitAsRange(1, 0, 5, D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

    // Descriptor table for samplers
    rootSignature[11].InitAsTable(1, D3D12_SHADER_VISIBILITY_PIXEL);
    rootSignature[11].InitAsRange(0, 0, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER,
        static_cast<UINT>(TextureFiltering::NUM_TEXTURE_FILTERINGS) * static_cast<UINT>(TextureAddressingMode::NUM_TEXTURE_ADDRESSING_MODES),
        D3D12_DESCRIPTOR_RANGE_FLAG_NONE);

    // Static samplers
    rootSignature.InitStaticSampler(0, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL, TextureFiltering::BILINEAR, TextureAddressingMode::BORDER, D3D12_COMPARISON_FUNC_GREATER_EQUAL);
    rootSignature.InitStaticSampler(1, 1, 1, D3D12_SHADER_VISIBILITY_PIXEL, TextureFiltering::BILINEAR, TextureAddressingMode::BORDER, D3D12_COMPARISON_FUNC_LESS_EQUAL);

    rootSignature.Finalize(m_device.Get());
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
        if (psoKey.passType == PassType::DEFERRED_LIGHTING)
        {
            depthStencilDesc.DepthEnable = FALSE;

            depthStencilDesc.StencilEnable = TRUE;
            depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
            depthStencilDesc.StencilWriteMask = 0;
            const D3D12_DEPTH_STENCILOP_DESC stencilOp =
            { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_EQUAL };
            depthStencilDesc.FrontFace = stencilOp;
            depthStencilDesc.BackFace = stencilOp;
        }
        else
        {
            depthStencilDesc.DepthEnable = TRUE;
            depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

            if (psoKey.passType == PassType::GBUFFER)
            {
                depthStencilDesc.StencilEnable = TRUE;
                depthStencilDesc.StencilReadMask = 0;
                depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
                const D3D12_DEPTH_STENCILOP_DESC stencilOp =
                { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_REPLACE, D3D12_COMPARISON_FUNC_ALWAYS };
                depthStencilDesc.FrontFace = stencilOp;
                depthStencilDesc.BackFace = stencilOp;
            }
            else
            {
                depthStencilDesc.StencilEnable = FALSE;
            }
        }

        // Describe and create the graphics pipeline state object (PSO).
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        if (psoKey.passType == PassType::DEFERRED_LIGHTING)
            psoDesc.InputLayout = { nullptr, 0 };
        else
            psoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };
        psoDesc.pRootSignature = m_rootSignature->GetRootSignature().Get();

        // Shader stages are selected by demand.
        // VS is essential for rasterization.
        // PS is optional. (e.g. Depth-only pass)
        std::wstring vsCsoName = Utility::RemoveFileExtension(psoKey.vsName) +
            (psoKey.passType == PassType::DEPTH_ONLY ? L"_depth_only" : L"") +
            L".cso";

        std::wstring psCsoName = Utility::RemoveFileExtension(psoKey.psName) + L".cso";

        const std::vector<char>& vsBlob = GetShaderBlobRef(ShaderKey{ vsCsoName });

        psoDesc.VS = { vsBlob.data(), vsBlob.size() };

        const std::vector<char>* psBlob = nullptr;

        if (!psoKey.psName.empty())
        {
            psBlob = &GetShaderBlobRef(ShaderKey{ psCsoName });
            psoDesc.PS = { psBlob->data(), psBlob->size() };
        }

        psoDesc.RasterizerState = rasterizerDesc;
        psoDesc.BlendState = blendDesc;
        psoDesc.DepthStencilState = depthStencilDesc;
        psoDesc.DSVFormat = psoKey.passType == PassType::DEPTH_ONLY ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        switch (psoKey.passType)
        {
        case PassType::FORWARD_COLORING:
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            break;
        case PassType::DEPTH_ONLY:
            if (psoKey.psName == L"PointLightShadowPS.hlsl")
            {
                psoDesc.NumRenderTargets = 1;
                psoDesc.RTVFormats[0] = DXGI_FORMAT_R32_FLOAT;
            }
            else
            {
                psoDesc.NumRenderTargets = 0;
            }
            break;
        case PassType::GBUFFER:
        {
            UINT numSlots = static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS);
            psoDesc.NumRenderTargets = numSlots;
            for (UINT i = 0; i < numSlots; ++i)
            {
                psoDesc.RTVFormats[i] = FrameResource::GetGBufferFormat(static_cast<GBufferSlot>(i));
            }
            break;
        }
        case PassType::DEFERRED_LIGHTING:
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
            break;
        }

        psoDesc.SampleDesc = { 1, 0 };
        ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&it->second)));
    }

    return it->second.Get();
}

const std::vector<char>& Renderer::GetShaderBlobRef(const ShaderKey& shaderKey) const
{
    auto it = m_shaderBlobs.find(shaderKey);

    if (it == m_shaderBlobs.end())
    {
        throw std::runtime_error("Shaders should be baked in initialization or first run.");
    }

    return it->second;
}

void Renderer::FixedUpdate(double fixedDtMs)
{
    float fixedDtSec = static_cast<float>(fixedDtMs) * 0.001f;

    if (m_inputManager.IsKeyPressed(VK_ESCAPE))
    {
        if (!PostMessageW(Win32Application::GetHwnd(), WM_CLOSE, 0, 0))
        {
            DWORD err = GetLastError();
            WCHAR buf[128];
            swprintf_s(buf, L"PostMessageW(WM_CLOSE) failed. GetLastError=%lu\n", err);
            OutputDebugStringW(buf);

            // fallback
            PostQuitMessage(static_cast<int>(err));
        }
    }

    if (m_inputManager.IsKeyPressed(VK_F11))
    {
        ToggleFullScreen();
    }

    if (m_inputManager.IsKeyPressed('V'))
    {
        m_vSync = !m_vSync;
    }

    m_inputManager.ResetKeyPressed();

    // Camera
    static float cameraMoveSpeed = 10.0f;

    float dist = cameraMoveSpeed * fixedDtSec;

    m_camera.SnapshotState();

    if (m_inputManager.IsKeyDown('W')) m_camera.MoveForward(dist);
    if (m_inputManager.IsKeyDown('A')) m_camera.MoveRight(-dist);
    if (m_inputManager.IsKeyDown('S')) m_camera.MoveForward(-dist);
    if (m_inputManager.IsKeyDown('D')) m_camera.MoveRight(dist);
    if (m_inputManager.IsKeyDown('Q')) m_camera.MoveUp(-dist);
    if (m_inputManager.IsKeyDown('E')) m_camera.MoveUp(dist);

    // RenderObjects
    static float rotationSpeed = 1.0f;  // unit : rad/s

    for (auto& [pMesh, objects] : m_forwardRenderObjects)
    {
        for (auto& ob : objects)
        {
            ob.SnapshotState();
        }
    }
    for (auto& [pMesh, objects] : m_deferredRenderObjects)
    {
        for (auto& ob : objects)
        {
            ob.SnapshotState();
        }
    }

    for (auto& ob : m_previewRotations)
    {
        ob->Transform(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, rotationSpeed * fixedDtSec, 0.0f), XMFLOAT3());
    }
}

void Renderer::PrepareConstantData(float alpha)
{
    // RenderObjects
    for (auto& [pMesh, objects] : m_forwardRenderObjects)
    {
        for (auto& ob : objects)
        {
            ob.UpdateRenderState(alpha);
        }
    }
    for (auto& [pMesh, objects] : m_deferredRenderObjects)
    {
        for (auto& ob : objects)
        {
            ob.UpdateRenderState(alpha);
        }
    }

    // Main Camera
    m_camera.UpdateRenderState(alpha);
    m_camera.Rotate(m_inputManager.GetAndResetMouseMove());
    m_cameraConstantData.SetPos(m_camera.GetPosition());
    m_cameraConstantData.SetView(m_camera.GetViewMatrix());
    m_cameraConstantData.SetProjection(m_camera.GetProjectionMatrix());

    // Light
    //XMVECTOR lightDir = m_lights[0]->GetDirection();
    //XMMATRIX rot = XMMatrixRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), 0.001f);
    //XMVECTOR rotated = XMVector3Transform(lightDir, rot);
    //m_lights[0]->SetDirection(rotated);

    // Pre-calculate common data for CSM.
    std::vector<BoundingSphere> cascadeSpheres = CalcCascadeSpheres();

    UINT lightIndices[3] = { 0, 0, 0 };

    // Wrap prepare... functions by lambda
    std::function<void(Light&)> prepareFuncs[3];
    prepareFuncs[0] = [&](Light& l) { PrepareDirectionalLight(static_cast<DirectionalLight&>(l), cascadeSpheres); };
    prepareFuncs[1] = [&](Light& l) { PreparePointLight(static_cast<PointLight&>(l)); };
    prepareFuncs[2] = [&](Light& l) { PrepareSpotLight(static_cast<SpotLight&>(l)); };

    for (UINT type = 0; type < static_cast<UINT>(LightType::NUM_LIGHT_TYPES); ++type)
    {
        for (auto& light : m_lights[type])
        {
            prepareFuncs[type](*light);
            light->SetIdxInArray(lightIndices[type]);
            ++lightIndices[type];
        }
    }
}

std::vector<BoundingSphere> Renderer::CalcCascadeSpheres()
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
    }

    return frustumBSs;
}

void Renderer::PrepareDirectionalLight(DirectionalLight& light, const std::vector<BoundingSphere>& cascadeSpheres)
{
    static XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMVECTOR cameraPos = m_camera.GetPosition();
    XMVECTOR dir = light.GetDirection();

    for (UINT i = 0; i < MAX_CASCADES; ++i)
    {
        XMVECTOR center = XMLoadFloat3(&cascadeSpheres[i].Center);
        float radius = cascadeSpheres[i].Radius;

        XMVECTOR viewOriginToCenter = center - cameraPos;

        // Calculate view/projection matrix fit to light frustum

        // Orthogonal projection of (center - view origin) onto lightDir.
        // This represents where the view origin is located relative to the center on the light's Z-axis.
        float d = XMVectorGetX(XMVector3Dot(viewOriginToCenter, dir));

        XMMATRIX view = XMMatrixLookToLH(center, dir, up);
        // Near Plane : Set to (view origin - sceneRadius) in Light Space.
        //              This ensures all shadow casters within 'sceneRadius' behind the camera are captured.
        // Far Plane :  Set to 'radius' to cover the entire bounding sphere of the view frustum.
        XMMATRIX projection = XMMatrixOrthographicLH(2 * radius, 2 * radius, radius, -d - m_camera.GetFarPlane());

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

        light.SetViewProjection(view, projection * fix, i);
    }
}

void Renderer::PreparePointLight(PointLight& light)
{
    XMVECTOR pos = light.GetPosition();

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

    // Set FOV as 90 degree
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, light.GetRange(), m_camera.GetNearPlane());

    for (UINT i = 0; i < POINT_LIGHT_ARRAY_SIZE; ++i)
    {
        XMMATRIX view = XMMatrixLookToLH(pos, Directions[i], Ups[i]);
        light.SetViewProjection(view, projection, i);
    }
}

void Renderer::PrepareSpotLight(SpotLight& light)
{
    static XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookToLH(light.GetPosition(), light.GetDirection(), up);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(light.GetOuterAngle(), 1.0f, light.GetRange(), m_camera.GetNearPlane());
    light.SetViewProjection(view, projection, 0);
}

void Renderer::UpdateConstantBuffers(FrameResource& frameResource)
{
    frameResource.UpdateCameraConstantBuffer(m_mainCameraIndex, &m_cameraConstantData);
    frameResource.UpdateShadowConstantBuffer(&m_shadowConstantData);

    for (UINT i = 0; i < m_materials.size(); ++i)
    {
        frameResource.UpdateMaterialConstantBuffer(i, m_materials[i]->GetConstantDataPtr());
    }

    UINT lightIdx = 0;
    for (UINT type = 0; type < static_cast<UINT>(LightType::NUM_LIGHT_TYPES); ++type)
    {
        UINT16 arraySize = GetRequiredArraySize(static_cast<LightType>(type));
        for (auto& light : m_lights[type])
        {
            for (UINT i = 0; i < arraySize; ++i)
            {
                frameResource.UpdateCameraConstantBuffer(light->GetCameraConstantBufferBaseIndex() + i, light->GetCameraConstantDataPtr(i));
            }
            frameResource.UpdateLightConstantBuffer(lightIdx, light->GetLightConstantDataPtr());
            ++lightIdx;
        }
    }
}

void Renderer::DrawMesh(ID3D12GraphicsCommandList7* pCommandList, Mesh& mesh, PassType passType, D3D12_GPU_VIRTUAL_ADDRESS instanceBufferBase)
{
    static const UINT instanceDataSize = static_cast<UINT>(sizeof(InstanceData));

    const auto& instanceRange = m_instanceRanges[&mesh];

    if ((passType == PassType::FORWARD_COLORING && instanceRange.forwardCount == 0) ||
        (passType == PassType::DEPTH_ONLY && (instanceRange.forwardCount + instanceRange.deferredCount) == 0) ||
        (passType == PassType::GBUFFER && instanceRange.deferredCount == 0) ||
        (passType == PassType::DEFERRED_LIGHTING))
        return;

    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    UINT instanceCount;
    switch (passType)
    {
    case PassType::FORWARD_COLORING:
        instanceCount = instanceRange.forwardCount;
        break;
    case PassType::DEPTH_ONLY:
        instanceCount = instanceRange.forwardCount + instanceRange.deferredCount;
        break;
    case PassType::GBUFFER:
        instanceCount = instanceRange.deferredCount;
        break;
    }

    D3D12_VERTEX_BUFFER_VIEW instanceBufferView;
    instanceBufferView.BufferLocation = instanceBufferBase + instanceRange.offset * instanceDataSize;
    instanceBufferView.StrideInBytes = instanceDataSize;
    instanceBufferView.SizeInBytes = instanceDataSize * instanceCount;

    D3D12_VERTEX_BUFFER_VIEW pVertexBufferViews[] = { mesh.GetVBV(), instanceBufferView };
    pCommandList->IASetVertexBuffers(0, 2, pVertexBufferViews);
    pCommandList->IASetIndexBuffer(&mesh.GetIBV());

    pCommandList->DrawIndexedInstanced(mesh.GetNumIndices(), instanceCount, 0, 0, 0);
}

template <>
DirectionalLight* Renderer::CreateLight<DirectionalLight>()
{
    auto dsvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(MAX_CASCADES);
    auto srvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate();
    auto cbvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount);

    auto light = std::make_unique<DirectionalLight>(
        m_device.Get(),
        std::move(dsvAllocation),
        std::move(srvAllocation),
        m_shadowMapResolution,
        *m_layoutTracker,
        m_frameResources,
        std::move(cbvAllocation));

    auto* pLight = light.get();
    m_lights[static_cast<UINT>(LightType::DIRECTIONAL)].push_back(std::move(light));

    return pLight;
}

template <>
PointLight* Renderer::CreateLight<PointLight>()
{
    auto dsvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(POINT_LIGHT_ARRAY_SIZE);
    auto srvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate();
    auto cbvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount);

    auto light = std::make_unique<PointLight>(
        m_device.Get(),
        std::move(dsvAllocation),
        std::move(srvAllocation),
        m_shadowMapResolution,
        *m_layoutTracker,
        m_frameResources,
        std::move(cbvAllocation),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate(POINT_LIGHT_ARRAY_SIZE));

    auto* pLight = light.get();
    m_lights[static_cast<UINT>(LightType::POINT)].push_back(std::move(light));

    return pLight;
}

template <>
SpotLight* Renderer::CreateLight<SpotLight>()
{
    auto dsvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(SPOT_LIGHT_ARRAY_SIZE);
    auto srvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate();
    auto cbvAllocation = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount);

    auto light = std::make_unique<SpotLight>(
        m_device.Get(),
        std::move(dsvAllocation),
        std::move(srvAllocation),
        m_shadowMapResolution,
        *m_layoutTracker,
        m_frameResources,
        std::move(cbvAllocation));

    auto* pLight = light.get();
    m_lights[static_cast<UINT>(LightType::SPOT)].push_back(std::move(light));

    return pLight;
}