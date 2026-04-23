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
#include "InstanceData.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"

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
    auto [pCommandAllocator, pCommandList] = m_commandQueue->GetAvailableCommandList();

    PopulateCommandList(pCommandList);

    m_dynamicDescriptorHeapForCBVSRVUAV->Reset();

    // Populate commands for ImGui
    // Is it OK to call SetDescriptorHeaps? (Does it affect performance?)
    ImGui::Render();
    ID3D12DescriptorHeap* ppHeaps[] = { m_imguiDescriptorAllocator->GetDescriptorHeap() };
    pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);

    // Barrier for RTV should be called after ImGui Render.
    // Swap Chain textures initially created in D3D12_BARRIER_LAYOUT_COMMON.
    // and presentation requires the back buffer is using D3D12_BARRIER_LAYOUT_COMMON.
    // LAYOUT_PRESENT is alias for LAYOUT_COMMON.
    D3D12_TEXTURE_BARRIER barrier = {
        D3D12_BARRIER_SYNC_RENDER_TARGET,
        D3D12_BARRIER_SYNC_NONE,
        D3D12_BARRIER_ACCESS_RENDER_TARGET,
        D3D12_BARRIER_ACCESS_NO_ACCESS,
        D3D12_BARRIER_LAYOUT_RENDER_TARGET,
        D3D12_BARRIER_LAYOUT_PRESENT,
        m_frameResources[m_frameIndex]->GetRenderTarget(),
        {0xffff'ffff, 0, 0, 0, 0, 0},
        D3D12_TEXTURE_BARRIER_FLAG_NONE
    };

    D3D12_BARRIER_GROUP barrierGroups[] = { TextureBarrierGroup(1, &barrier) };
    pCommandList->Barrier(1, barrierGroups);

    // Execute the command lists and store the fence value
    UINT64 fenceValue = m_commandQueue->ExecuteCommandLists(pCommandAllocator, pCommandList);
    m_frameResources[m_frameIndex]->SetFenceValue(fenceValue);
    m_dynamicDescriptorHeapForCBVSRVUAV->QueueRetiredHeaps(fenceValue);
    m_sceneManager.QueueDeferredDeletions(fenceValue, m_commandQueue->GetCompletedFenceValue());

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
        m_frameResources[i]->ResetRenderTarget();

        // Release previous gbuffers and create new ones
        for (UINT j = 0; j < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++j)
        {
            auto slot = static_cast<GBufferSlot>(j);
            m_frameResources[i]->ResetGBuffer(slot);
        }
        m_frameResources[i]->CreateGBuffers(m_width, m_height);

        m_frameResources[i]->SetFenceValue(m_frameResources[m_frameIndex]->GetFenceValue());
    }
    m_depthStencilBuffer.Reset();

    // Preserve existing format
    // Before calling ResizeBuffers, all backbuffer references should be released.
    ThrowIfFailed(m_swapChain->ResizeBuffers(FrameCount, m_width, m_height, DXGI_FORMAT_UNKNOWN, m_tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0));
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    // Recreate RTVs
    for (UINT i = 0; i < FrameCount; i++)
    {
        m_frameResources[i]->AcquireBackBuffer(m_swapChain.Get(), i);
        CreateRTV(m_device.Get(), m_frameResources[i]->GetRenderTarget(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, m_frameResources[i]->GetRTVHandle());
    }

    // Recreate depth-stencil buffer, DSV, and SRV
    CreateDepthStencilBuffer(m_device.Get(), m_width, m_height, 1, m_depthStencilBuffer, true);
    CreateDSV(m_device.Get(), m_depthStencilBuffer.Get(), m_dsvAllocation.GetDescriptorHandle(), true, false);
    CreateDSV(m_device.Get(), m_depthStencilBuffer.Get(), m_readOnlyDSVAllocation.GetDescriptorHandle(), true, true);
    CreateSRV(m_device.Get(), m_depthStencilBuffer.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, m_depthSRVAllocation.GetDescriptorHandle());

    // Update registered info of backbuffers
    std::vector<ID3D12Resource*> pBackBuffers;
    for (UINT i = 0; i < FrameCount; ++i)
    {
        pBackBuffers.push_back(m_frameResources[i]->GetRenderTarget());
    }
    auto backBuffer = m_renderGraph.GetRGTexture("BackBuffer");
    m_renderGraph.UpdateElement(backBuffer, 0, pBackBuffers);

    // Update registered info of depth-stencil buffer
    auto depthStencilBuffer = m_renderGraph.GetRGTexture("DepthStencilBuffer");
    m_renderGraph.UpdateElement(depthStencilBuffer, 0, { m_depthStencilBuffer.Get() });

    // Update registered info of GBuffers
    auto gBuffer = m_renderGraph.GetRGTexture("GBuffer");
    for (UINT slot = 0; slot < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++slot)
    {
        std::vector<ID3D12Resource*> pGBuffers;
        for (UINT i = 0; i < FrameCount; ++i)
        {
            pGBuffers.push_back(m_frameResources[i]->GetGBuffer(static_cast<GBufferSlot>(slot)));
        }
        m_renderGraph.UpdateElement(gBuffer, slot, pGBuffers);
    }
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

    if (ImGui::Button("Add Cube"))
    {
        auto hMesh = m_sceneManager.GetMeshHandle("builtin://mesh/cube");
        auto hTemplateMat = m_sceneManager.GetMaterialHandle("PavingStones150");
        auto hMat = CloneMaterial(hTemplateMat);

        auto hCube = m_sceneManager.AddEntity("New Cube");
        m_sceneManager.AddTransform(hCube);
        m_sceneManager.SetMesh(hCube, hMesh);
        m_sceneManager.SetMaterial(hCube, hMat);
    }

    ImGui::End();

    bool selectionChanged = false;

    // Hierarchy
    ImGui::Begin("Hierarchy");

    static EntityHandle selected;
    EntityHandle toDelete;

    if (ImGui::IsKeyPressed(ImGuiKey_Delete))
        toDelete = selected;

    for (const auto& entity : m_sceneManager.GetEntities())
    {
        if (entity.parent.index == UINT_MAX && entity.parent.generation == 0)
            RenderEntityNode(entity, selected, toDelete, selectionChanged);
    }

    m_sceneManager.Remove(toDelete);
    if (selected == toDelete)
        selected = {};

    ImGui::End();

    // Inspector
    ImGui::Begin("Inspector");

    auto* pEntity = m_sceneManager.Get(selected);
    if (pEntity)
    {
        if (pEntity->transform.has_value())
        {
            auto& transform = pEntity->transform.value();

            XMFLOAT3 s = transform.GetScale();
            if (ImGui::DragFloat3("Scale", &s.x))
                transform.SetScale(s);

            XMFLOAT3 eulerR = transform.GetEulerCache(selectionChanged);
            if (ImGui::DragFloat3("Rotation", &eulerR.x))
            {
                transform.SetRotation(eulerR);
            }

            XMFLOAT3 t = transform.GetTranslation();
            if (ImGui::DragFloat3("Translation", &t.x))
                transform.SetTranslation(t);
        }
    }

    ImGui::End();
}

void Renderer::RenderEntityNode(const Entity& entity, EntityHandle& selected, EntityHandle& toDelete, bool& selectionChanged)
{
    bool isSelected = (entity.selfHandle == selected);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (entity.children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;

    UINT64 id = (static_cast<UINT64>(entity.selfHandle.index) << 32) | entity.selfHandle.generation;
    bool isExpanded = ImGui::TreeNodeEx(reinterpret_cast<void*>(id), flags, "%s", entity.name.c_str());

    if (ImGui::IsItemClicked() && selected != entity.selfHandle)
    {
        selected = entity.selfHandle;
        selectionChanged = true;
    }

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Delete"))
            toDelete = entity.selfHandle;
        ImGui::EndPopup();
    }
    if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen) && isExpanded)
    {
        for (auto c : entity.children)
            RenderEntityNode(*m_sceneManager.Get(c), selected, toDelete, selectionChanged);
        ImGui::TreePop();
    }
}

void Renderer::LoadPipeline()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Enable the D3D12 debug layer and GBV
    {
        ComPtr<ID3D12Debug1> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();
            debugController->SetEnableGPUBasedValidation(TRUE);
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
        GetHardwareAdapter(factory.Get(), hardwareAdapter.GetAddressOf());

        ThrowIfFailed(D3D12CreateDevice(
            hardwareAdapter.Get(),
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_device)
        ));
    }

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
        m_commandQueue->GetCommandQueue(),
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
    TransientUploadAllocator uploadAllocator(m_device.Get());

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

    CreateDSV(m_device.Get(), m_depthStencilBuffer.Get(), m_dsvAllocation.GetDescriptorHandle(), true, false);
    CreateDSV(m_device.Get(), m_depthStencilBuffer.Get(), m_readOnlyDSVAllocation.GetDescriptorHandle(), true, true);
    CreateSRV(m_device.Get(), m_depthStencilBuffer.Get(), DXGI_FORMAT_R24_UNORM_X8_TYPELESS, m_depthSRVAllocation.GetDescriptorHandle());

    // Set viewport and scissorRect for shadow mapping
    m_shadowMapViewport = { 0.0f, 0.0f, static_cast<float>(m_shadowMapResolution), static_cast<float>(m_shadowMapResolution), 0.0f, 1.0f };
    m_shadowMapScissorRect = { 0, 0, static_cast<LONG>(m_shadowMapResolution), static_cast<LONG>(m_shadowMapResolution) };

    // Get command allocator and list for loading assets
    auto [pCommandAllocator, pCommandList] = m_commandQueue->GetAvailableCommandList();

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
    // Default textures
    {
        auto allocations = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(3).Split();

        // index 0: white albedo
        CreateTexture(pCommandList, std::move(allocations[0]), uploadAllocator, { 255, 255, 255, 255 }, 1, 1);

        // index 1: flat normal  (128, 128, 255) in linear space
        CreateTexture(pCommandList, std::move(allocations[1]), uploadAllocator, { 128, 128, 255, 255 }, 1, 1);

        // index 2: black height
        CreateTexture(pCommandList, std::move(allocations[2]), uploadAllocator, { 0, 0, 0, 255 }, 1, 1);

        auto hDefaultMat = CreateMaterial("builtin://material/default");
        auto* pDefaultMat = m_sceneManager.GetMaterial(hDefaultMat);
        pDefaultMat->SetAmbient(XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f));
        pDefaultMat->SetSpecular(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
        pDefaultMat->SetShininess(1.0f);
        pDefaultMat->SetTextureIndices(0, 1, 2);   // default textures
        pDefaultMat->BuildSamplerIndices(m_currentTextureFiltering);
        pDefaultMat->SetRenderingPath(RenderingPath::DEFERRED);
    }

    auto allocations = m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(3).Split();

    CreateTexture(
        pCommandList,
        std::move(allocations[0]),
        uploadAllocator,
        L"assets/textures/PavingStones150_4K-PNG_Color.png",
        true,
        true,
        false,
        false);

    CreateTexture(
        pCommandList,
        std::move(allocations[1]),
        uploadAllocator,
        L"assets/textures/PavingStones150_4K-PNG_NormalDX.png",
        false,
        true,
        false,
        false);

    CreateTexture(
        pCommandList,
        std::move(allocations[2]),
        uploadAllocator,
        L"assets/textures/PavingStones150_4K-PNG_Displacement.png",
        false,
        true,
        false,
        false);

    // Add materials
    auto hBaseMat = CreateMaterial("PavingStones150");
    auto* pBaseMat = m_sceneManager.GetMaterial(hBaseMat);
    pBaseMat->SetAmbient(XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f));
    pBaseMat->SetSpecular(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
    pBaseMat->SetShininess(10.0f);
    pBaseMat->SetTextureIndices(3, 4, 5);
    pBaseMat->SetTextureAddressingModes(TextureAddressingMode::WRAP, TextureAddressingMode::WRAP, TextureAddressingMode::WRAP);
    pBaseMat->BuildSamplerIndices(m_currentTextureFiltering);
    pBaseMat->SetRenderingPath(RenderingPath::DEFERRED);

    auto hPlaneMat = CloneMaterial(hBaseMat);
    auto* pPlaneMat = m_sceneManager.GetMaterial(hPlaneMat);
    pPlaneMat->SetTextureTileScales(50.0f, 50.0f, 50.0f);

    // Add meshes
    auto hCubeMesh = m_sceneManager.AddMesh(m_device.Get(), pCommandList, uploadAllocator, GeometryGenerator::GenerateCube());
    auto hSphereMesh = m_sceneManager.AddMesh(m_device.Get(), pCommandList, uploadAllocator, GeometryGenerator::GenerateSphere());

    // Add Entities
    auto hPlane = m_sceneManager.AddEntity("Plane");
    m_sceneManager.AddTransform(hPlane, XMFLOAT3(1000.0f, 0.5f, 1000.0f), XMFLOAT3(), XMFLOAT3(0.0f, -5.0f, 0.0f));
    m_sceneManager.SetMesh(hPlane, hCubeMesh);
    m_sceneManager.SetMaterial(hPlane, hPlaneMat);

    auto hFolder = m_sceneManager.AddEntity("Folder");
    m_sceneManager.AddTransform(hFolder);
    for (UINT i = 0; i < 10; i++)
    {
        for (UINT j = 0; j < 10; j++)
        {
            auto hCube = m_sceneManager.AddEntity("Cube");
            m_sceneManager.AddTransform(hCube, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(), XMFLOAT3((i - 5.0f) * 4.0f, j * 4.0f, 10.0f));
            m_sceneManager.SetMesh(hCube, hCubeMesh);
            m_sceneManager.SetMaterial(hCube, hBaseMat);

            m_previewRotations.push_back(hCube);
            m_sceneManager.AddChild(hFolder, hCube);
        }
    }

    auto hSphere = m_sceneManager.AddEntity("Sphere");
    m_sceneManager.AddTransform(hSphere, XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(), XMFLOAT3(0.0f, -3.5f, 0.0f));
    m_sceneManager.SetMesh(hSphere, hSphereMesh);
    m_sceneManager.SetMaterial(hSphere, hBaseMat);

    // Set up lights
    auto hDirectionalLight = m_sceneManager.AddEntity("DirectionalLight");
    auto hDirectionalLightComponent = CreateDirectionalLight();
    m_sceneManager.AddComponent(hDirectionalLight, hDirectionalLightComponent);
    auto* pDirectionalLight = m_sceneManager.Get(hDirectionalLightComponent);
    pDirectionalLight->SetDirection(XMFLOAT3(-1.0f, -1.0f, 1.0f));

    auto hPointLight = m_sceneManager.AddEntity("PointLight");
    auto hPointLightComponent = CreatePointLight();
    m_sceneManager.AddComponent(hPointLight, hPointLightComponent);
    auto* pPointLight = m_sceneManager.Get(hPointLightComponent);
    pPointLight->SetPosition(XMFLOAT3(0.0f, 4.0f, 3.0f));
    pPointLight->SetRange(30.0f);

    auto hSpotLight = m_sceneManager.AddEntity("SpotLight");
    auto hSpotLightComponent = CreateSpotLight();
    m_sceneManager.AddComponent(hSpotLight, hSpotLightComponent);
    auto* pSpotLight = m_sceneManager.Get(hSpotLightComponent);
    pSpotLight->SetPosition(XMFLOAT3(0.0f, 10.0f, -5.0f));
    pSpotLight->SetDirection(XMFLOAT3(0.0f, -1.0f, 1.0f));
    pSpotLight->SetRange(50.0f);
    pSpotLight->SetAngles(50.0f, 10.0f);

    // Execute commands for loading assets and store fence value
    m_frameResources[m_frameIndex]->SetFenceValue(m_commandQueue->ExecuteCommandLists(pCommandAllocator, pCommandList));

    // Wait until assets have been uploaded to the GPU
    WaitForGPU();

    // Render Graph
    m_renderGraph.Init(m_device.Get(), FrameCount);

    // Back buffer
    RGTexture backBuffer = m_renderGraph.RegisterTexture(
        "BackBuffer",
        true,
        { D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_PRESENT },
        GetSubresourceCount(m_device.Get(), m_frameResources.front()->GetRenderTarget()));
    std::vector<ID3D12Resource*> pBackBuffers;
    for (UINT i = 0; i < FrameCount; ++i)
    {
        pBackBuffers.push_back(m_frameResources[i]->GetRenderTarget());
    }
    m_renderGraph.AddElement(backBuffer, pBackBuffers);

    // Depth-stencil buffer
    RGTexture depthStencilBuffer = m_renderGraph.RegisterTexture(
        "DepthStencilBuffer",
        false,
        { D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE },
        GetSubresourceCount(m_device.Get(), m_depthStencilBuffer.Get()));
    m_renderGraph.AddElement(depthStencilBuffer, { m_depthStencilBuffer.Get() });

    // GBuffer
    RGTexture gBuffer = m_renderGraph.RegisterTexture(
        "GBuffer",
        true,
        { D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_RENDER_TARGET },
        GetSubresourceCount(m_device.Get(), m_frameResources.front()->GetGBuffer(GBufferSlot::ALBEDO)));
    for (UINT slot = 0; slot < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++slot)
    {
        std::vector<ID3D12Resource*> pGBuffers;
        for (UINT i = 0; i < FrameCount; ++i)
        {
            pGBuffers.push_back(m_frameResources[i]->GetGBuffer(static_cast<GBufferSlot>(slot)));
        }
        m_renderGraph.AddElement(gBuffer, pGBuffers);
    }

    // Light
    m_renderGraph.RegisterTexture(
        "DirectionalLight",
        false,
        { D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE },
        GetSubresourceCount(m_device.Get(), GetDepthStencilBufferDesc(m_shadowMapResolution, m_shadowMapResolution, MAX_CASCADES, false)),
        [this]()
        {
            std::vector<ID3D12Resource*> pResources;
            for (auto& light : m_sceneManager.GetDirectionalLights())
                pResources.push_back(light.GetDepthBuffer());
            return pResources;
        }
    );

    m_renderGraph.RegisterTexture(
        "PointLight",
        false,
        { D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_RENDER_TARGET },
        GetSubresourceCount(m_device.Get(), GetRenderTargetDesc(m_shadowMapResolution, m_shadowMapResolution, POINT_LIGHT_ARRAY_SIZE, DXGI_FORMAT_R32_TYPELESS)),
        [this]()
        {
            std::vector<ID3D12Resource*> pResources;
            for (auto& light : m_sceneManager.GetPointLights())
                pResources.push_back(light.GetRenderTarget());
            return pResources;
        }
    );

    m_renderGraph.RegisterTexture(
        "SpotLight",
        false,
        { D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE },
        GetSubresourceCount(m_device.Get(), GetDepthStencilBufferDesc(m_shadowMapResolution, m_shadowMapResolution, SPOT_LIGHT_ARRAY_SIZE, false)),
        [this]()
        {
            std::vector<ID3D12Resource*> pResources;
            for (auto& light : m_sceneManager.GetSpotLights())
                pResources.push_back(light.GetDepthBuffer());
            return pResources;
        }
    );

    PrepareRenderGraph();
    m_renderGraph.Compile();
}

void Renderer::PopulateCommandList(ID3D12GraphicsCommandList7* pCommandList)
{
    PIX_SCOPED_EVENT(pCommandList, PIX_COLOR_DEFAULT, L"PopulateCommandList");

    FrameResource& frameResource = *m_frameResources[m_frameIndex];
    frameResource.ResetInstanceOffsetByte();

    // Set root signature
    pCommandList->SetGraphicsRootSignature(m_rootSignature->GetRootSignature());

    UINT numLights = m_sceneManager.GetLightCount();
    pCommandList->SetGraphicsRoot32BitConstant(2, numLights, 0);

    // Stage material CBVs
    UINT matIdx = 0;
    for (auto& mat : m_sceneManager.GetMaterials())
    {
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(4, matIdx, 1, mat.GetCBVAllocationRef(m_frameIndex));
        ++matIdx;
    }

    // Stage light CBVs
    UINT lightIdx = 0;
    for (auto& light : m_sceneManager.GetDirectionalLights())
    {
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(5, lightIdx, 1, light.GetLightCBVAllocationRef(m_frameIndex));
        ++lightIdx;
    }
    for (auto& light : m_sceneManager.GetPointLights())
    {
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(5, lightIdx, 1, light.GetLightCBVAllocationRef(m_frameIndex));
        ++lightIdx;
    }
    for (auto& light : m_sceneManager.GetSpotLights())
    {
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(5, lightIdx, 1, light.GetLightCBVAllocationRef(m_frameIndex));
        ++lightIdx;
    }

    // Stage textures
    UINT textureIdx = 0;
    for (auto& texture : m_sceneManager.GetTextures())
    {
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(6, textureIdx, 1, texture.GetAllocationRef());
        ++textureIdx;
    }

    // Stage shadow SRVs
    lightIdx = 0;
    for (auto& light : m_sceneManager.GetDirectionalLights())
    {
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(7, lightIdx, 1, light.GetSRVAllocationRef());
        ++lightIdx;
    }
    lightIdx = 0;
    for (auto& light : m_sceneManager.GetPointLights())
    {
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(8, lightIdx, 1, light.GetSRVAllocationRef());
        ++lightIdx;
    }
    lightIdx = 0;
    for (auto& light : m_sceneManager.GetSpotLights())
    {
        m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(9, lightIdx, 1, light.GetSRVAllocationRef());
        ++lightIdx;
    }

    m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(10, 0, static_cast<UINT32>(GBufferSlot::NUM_GBUFFER_SLOTS), frameResource.GetGBufferSRVAllocationRef());
    m_dynamicDescriptorHeapForCBVSRVUAV->StageDescriptors(10, static_cast<UINT32>(GBufferSlot::NUM_GBUFFER_SLOTS), 1, m_depthSRVAllocation);

    BindDescriptorTables(pCommandList);

    auto data = m_sceneManager.GatherInstances();
    frameResource.EnsureInstanceCapacity(static_cast<UINT>(data.size()));
    frameResource.PushInstanceData(data);

    // Depth-only pass for shadow mapping
    // Also work as z-prepass
    {
        PIX_SCOPED_EVENT(pCommandList, PIX_COLOR_DEFAULT, L"Depth-only pass");

        ApplyPassBarriers(m_renderGraph, PassType::DEPTH_ONLY, pCommandList);

        pCommandList->RSSetViewports(1, &m_shadowMapViewport);
        pCommandList->RSSetScissorRects(1, &m_shadowMapScissorRect);

        // Pre-query PSOs
        m_currentPSOKey.passType = PassType::DEPTH_ONLY;
        m_currentPSOKey.vsName = L"vs.hlsl";
        m_currentPSOKey.psName = L"";
        auto* shadowPSO = GetPipelineState(m_currentPSOKey);

        m_currentPSOKey.psName = L"PointLightShadowPS.hlsl";
        auto* pointShadowPSO = GetPipelineState(m_currentPSOKey);

        auto processLight = [&](Light* pLight, bool isPointLight, UINT& lightIdx)
            {
                if (isPointLight) pCommandList->SetGraphicsRoot32BitConstant(3, lightIdx, 0);

                // Render each entry of shadow map.
                UINT16 arraySize = pLight->GetArraySize();
                for (UINT j = 0; j < arraySize; ++j)
                {
                    auto shadowMapDsvHandle = pLight->GetDSVHandle(j);

                    if (isPointLight)
                    {
                        auto rtvHandle = static_cast<PointLight*>(pLight)->GetRTVHandle(j);
                        pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &shadowMapDsvHandle);

                        XMVECTORF32 clearColor;
                        clearColor.v = XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
                        pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
                    }
                    else
                    {
                        pCommandList->OMSetRenderTargets(0, nullptr, FALSE, &shadowMapDsvHandle);
                    }

                    pCommandList->ClearDepthStencilView(shadowMapDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 0.0f, 0, 0, nullptr);

                    pCommandList->SetPipelineState(isPointLight ? pointShadowPSO : shadowPSO);

                    pCommandList->SetGraphicsRootConstantBufferView(0, pLight->GetCameraUploadAllocation(j).GPUPtr);

                    for (const auto& [meshHandle, bucket] : m_sceneManager.GetBuckets())
                    {
                        DrawMesh(pCommandList, meshHandle, PassType::DEPTH_ONLY, frameResource.GetInstanceBufferVirtualAddress());
                    }
                }

                ++lightIdx;
            };

        UINT lightIdx = 0;
        for (auto& light : m_sceneManager.GetDirectionalLights())
        {
            processLight(&light, false, lightIdx);
        }
        for (auto& light : m_sceneManager.GetPointLights())
        {
            processLight(&light, true, lightIdx);
        }
        for (auto& light : m_sceneManager.GetSpotLights())
        {
            processLight(&light, false, lightIdx);
        }
    }

    // GBuffer pass
    {
        PIX_SCOPED_EVENT(pCommandList, PIX_COLOR_DEFAULT, L"GBuffer pass");

        ApplyPassBarriers(m_renderGraph, PassType::GBUFFER, pCommandList);

        pCommandList->RSSetViewports(1, &m_viewport);
        pCommandList->RSSetScissorRects(1, &m_scissorRect);

        // Pre-query PSOs
        m_currentPSOKey.passType = PassType::GBUFFER;
        m_currentPSOKey.vsName = L"vs.hlsl";
        m_currentPSOKey.psName = L"GBufferPS.hlsl";
        auto* pso = GetPipelineState(m_currentPSOKey);

        auto rtvHandles = frameResource.GetGBufferRTVHandles();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvAllocation.GetDescriptorHandle();
        pCommandList->OMSetRenderTargets(static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS), rtvHandles.data(), FALSE, &dsvHandle);

        XMVECTORF32 clearColor;
        clearColor.v = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
        for (auto& rtvHandle : rtvHandles)
            pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        pCommandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);
        pCommandList->OMSetStencilRef(1);

        pCommandList->SetPipelineState(pso);

        pCommandList->SetGraphicsRootConstantBufferView(0, m_cameraUploadAllocation.GPUPtr);

        for (const auto& [meshHandle, bucket] : m_sceneManager.GetBuckets())
        {
            DrawMesh(pCommandList, meshHandle, PassType::GBUFFER, frameResource.GetInstanceBufferVirtualAddress());
        }
    }

    // Deferred Lighting pass
    {
        PIX_SCOPED_EVENT(pCommandList, PIX_COLOR_DEFAULT, L"Deferred Lighting pass");

        ApplyPassBarriers(m_renderGraph, PassType::DEFERRED_LIGHTING, pCommandList);

        pCommandList->RSSetViewports(1, &m_viewport);
        pCommandList->RSSetScissorRects(1, &m_scissorRect);

        // Pre-query PSOs
        m_currentPSOKey.passType = PassType::DEFERRED_LIGHTING;
        m_currentPSOKey.vsName = L"DeferredLightingVS.hlsl";
        m_currentPSOKey.psName = L"DeferredLightingPS.hlsl";
        auto* pso = GetPipelineState(m_currentPSOKey);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = frameResource.GetRTVHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_readOnlyDSVAllocation.GetDescriptorHandle();
        pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);
        pCommandList->OMSetStencilRef(1);

        // Use linear color for gamma-correct rendering
        XMVECTORF32 clearColor;
        clearColor.v = XMColorSRGBToRGB(XMVectorSet(0.5f, 0.5f, 0.5f, 1.0f));
        pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        pCommandList->SetPipelineState(pso);

        pCommandList->SetGraphicsRootConstantBufferView(0, m_cameraUploadAllocation.GPUPtr);
        pCommandList->SetGraphicsRootConstantBufferView(1, m_shadowUploadAllocation.GPUPtr);

        pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        pCommandList->DrawInstanced(3, 1, 0, 0);

        std::vector<D3D12_TEXTURE_BARRIER> barriers;
        for (UINT slot = 0; slot < static_cast<UINT>(GBufferSlot::NUM_GBUFFER_SLOTS); ++slot)
        {
            D3D12_TEXTURE_BARRIER b = {
                D3D12_BARRIER_SYNC_PIXEL_SHADING,
                D3D12_BARRIER_SYNC_NONE,
                D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
                D3D12_BARRIER_ACCESS_NO_ACCESS,
                D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
                D3D12_BARRIER_LAYOUT_RENDER_TARGET,
                frameResource.GetGBuffer(static_cast<GBufferSlot>(slot)),
                {0xffff'ffff, 0, 0, 0, 0, 0},
                D3D12_TEXTURE_BARRIER_FLAG_NONE
            };
            barriers.push_back(b);
        }
        D3D12_BARRIER_GROUP barrierGroups[] = { TextureBarrierGroup(static_cast<UINT32>(barriers.size()), barriers.data()) };
        pCommandList->Barrier(1, barrierGroups);
    }

    // Forward Coloring pass
    {
        PIX_SCOPED_EVENT(pCommandList, PIX_COLOR_DEFAULT, L"Forward color pass");

        ApplyPassBarriers(m_renderGraph, PassType::FORWARD_COLORING, pCommandList);

        pCommandList->RSSetViewports(1, &m_viewport);
        pCommandList->RSSetScissorRects(1, &m_scissorRect);

        // Pre-query PSOs
        m_currentPSOKey.passType = PassType::FORWARD_COLORING;
        m_currentPSOKey.vsName = L"vs.hlsl";
        m_currentPSOKey.psName = L"ps.hlsl";
        auto* pso = GetPipelineState(m_currentPSOKey);

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = frameResource.GetRTVHandle();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvAllocation.GetDescriptorHandle();
        pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

        pCommandList->SetPipelineState(pso);

        pCommandList->SetGraphicsRootConstantBufferView(0, m_cameraUploadAllocation.GPUPtr);
        pCommandList->SetGraphicsRootConstantBufferView(1, m_shadowUploadAllocation.GPUPtr);

        for (const auto& [meshHandle, bucket] : m_sceneManager.GetBuckets())
        {
            DrawMesh(pCommandList, meshHandle, PassType::FORWARD_COLORING, frameResource.GetInstanceBufferVirtualAddress());
        }

        std::vector<D3D12_TEXTURE_BARRIER> barriers;

        auto processLight = [&](Light* pLight, bool isPointLight)
            {
                if (isPointLight)
                {
                    D3D12_TEXTURE_BARRIER b = {
                        D3D12_BARRIER_SYNC_PIXEL_SHADING,
                        D3D12_BARRIER_SYNC_NONE,
                        D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
                        D3D12_BARRIER_ACCESS_NO_ACCESS,
                        D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
                        D3D12_BARRIER_LAYOUT_RENDER_TARGET,
                        static_cast<PointLight*>(pLight)->GetRenderTarget(),
                        {0xffff'ffff, 0, 0, 0, 0, 0},
                        D3D12_TEXTURE_BARRIER_FLAG_NONE
                    };
                    barriers.push_back(b);
                }
                else
                {
                    D3D12_TEXTURE_BARRIER b = {
                        D3D12_BARRIER_SYNC_PIXEL_SHADING,
                        D3D12_BARRIER_SYNC_NONE,
                        D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
                        D3D12_BARRIER_ACCESS_NO_ACCESS,
                        D3D12_BARRIER_LAYOUT_SHADER_RESOURCE,
                        D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE,
                        pLight->GetDepthBuffer(),
                        {0xffff'ffff, 0, 0, 0, 0, 0},
                        D3D12_TEXTURE_BARRIER_FLAG_NONE
                    };
                    barriers.push_back(b);
                }
            };

        for (auto& light : m_sceneManager.GetDirectionalLights())
        {
            processLight(&light, false);
        }
        for (auto& light : m_sceneManager.GetPointLights())
        {
            processLight(&light, true);
        }
        for (auto& light : m_sceneManager.GetSpotLights())
        {
            processLight(&light, false);
        }

        D3D12_BARRIER_GROUP barrierGroups[] = { TextureBarrierGroup(static_cast<UINT32>(barriers.size()), barriers.data()) };
        pCommandList->Barrier(1, barrierGroups);
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
    init_info.CommandQueue = m_commandQueue->GetCommandQueue();
    init_info.NumFramesInFlight = FrameCount;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    init_info.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    init_info.SrvDescriptorHeap = m_imguiDescriptorAllocator->GetDescriptorHeap();
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return ImGuiSrvDescriptorAllocate(out_cpu_handle, out_gpu_handle); };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return ImGuiSrvDescriptorFree(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&init_info);

    ImGui::GetStyle().FontScaleMain = m_dpiScale;
}

void Renderer::PrepareRenderGraph()
{
    // Set up render graph
    auto& depthOnlyPass = m_renderGraph.m_nodes[static_cast<UINT>(PassType::DEPTH_ONLY)];
    auto& gBufferPass = m_renderGraph.m_nodes[static_cast<UINT>(PassType::GBUFFER)];
    auto& deferredLightingPass = m_renderGraph.m_nodes[static_cast<UINT>(PassType::DEFERRED_LIGHTING)];
    auto& forwardColoringPass = m_renderGraph.m_nodes[static_cast<UINT>(PassType::FORWARD_COLORING)];

    // Resource handles
    auto backBuffer = m_renderGraph.GetRGTexture("BackBuffer");
    auto depthStencilBuffer = m_renderGraph.GetRGTexture("DepthStencilBuffer");
    auto gBuffer = m_renderGraph.GetRGTexture("GBuffer");
    auto directionalLightDepthBuffer = m_renderGraph.GetRGTexture("DirectionalLight");
    auto pointLightRenderTarget = m_renderGraph.GetRGTexture("PointLight");
    auto spotLightDepthBuffer = m_renderGraph.GetRGTexture("SpotLight");

    // Depth-only pass
    depthOnlyPass.AddTextureInput(directionalLightDepthBuffer, { D3D12_BARRIER_SYNC_DEPTH_STENCIL ,D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
    depthOnlyPass.AddTextureOutput(directionalLightDepthBuffer, { D3D12_BARRIER_SYNC_DEPTH_STENCIL ,D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
    depthOnlyPass.AddTextureInput(pointLightRenderTarget, { D3D12_BARRIER_SYNC_RENDER_TARGET ,D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    depthOnlyPass.AddTextureOutput(pointLightRenderTarget, { D3D12_BARRIER_SYNC_RENDER_TARGET ,D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    depthOnlyPass.AddTextureInput(spotLightDepthBuffer, { D3D12_BARRIER_SYNC_DEPTH_STENCIL ,D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
    depthOnlyPass.AddTextureOutput(spotLightDepthBuffer, { D3D12_BARRIER_SYNC_DEPTH_STENCIL ,D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });

    // GBuffer pass
    gBufferPass.AddTextureInput(depthStencilBuffer, { D3D12_BARRIER_SYNC_DEPTH_STENCIL, D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
    gBufferPass.AddTextureOutput(depthStencilBuffer, { D3D12_BARRIER_SYNC_DEPTH_STENCIL, D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
    gBufferPass.AddTextureInput(gBuffer, { D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    gBufferPass.AddTextureOutput(gBuffer, { D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_LAYOUT_RENDER_TARGET });

    // Deferred lighting pass
    deferredLightingPass.AddTextureInput(backBuffer, { D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    deferredLightingPass.AddTextureOutput(backBuffer, { D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    // To use depth-stencil buffer as both SRV and DSV simultaneously
    // SRV: to reconstruct world pos
    // DSV: for stencil test
    deferredLightingPass.AddTextureInput(depthStencilBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING, D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ });
    deferredLightingPass.AddTextureOutput(depthStencilBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING, D3D12_BARRIER_ACCESS_SHADER_RESOURCE | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ, D3D12_BARRIER_LAYOUT_DIRECT_QUEUE_GENERIC_READ });
    deferredLightingPass.AddTextureInput(gBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING, D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    deferredLightingPass.AddTextureOutput(gBuffer, { D3D12_BARRIER_SYNC_NONE, D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    deferredLightingPass.AddTextureInput(directionalLightDepthBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    deferredLightingPass.AddTextureOutput(directionalLightDepthBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    deferredLightingPass.AddTextureInput(pointLightRenderTarget, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    deferredLightingPass.AddTextureOutput(pointLightRenderTarget, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    deferredLightingPass.AddTextureInput(spotLightDepthBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    deferredLightingPass.AddTextureOutput(spotLightDepthBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });

    // Forward coloring pass
    forwardColoringPass.AddTextureInput(backBuffer, { D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    forwardColoringPass.AddTextureOutput(backBuffer, { D3D12_BARRIER_SYNC_RENDER_TARGET, D3D12_BARRIER_ACCESS_RENDER_TARGET, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    forwardColoringPass.AddTextureInput(depthStencilBuffer, { D3D12_BARRIER_SYNC_DEPTH_STENCIL, D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
    forwardColoringPass.AddTextureOutput(depthStencilBuffer, { D3D12_BARRIER_SYNC_DEPTH_STENCIL, D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
    forwardColoringPass.AddTextureInput(directionalLightDepthBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    forwardColoringPass.AddTextureOutput(directionalLightDepthBuffer, { D3D12_BARRIER_SYNC_NONE ,D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
    forwardColoringPass.AddTextureInput(pointLightRenderTarget, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    forwardColoringPass.AddTextureOutput(pointLightRenderTarget, { D3D12_BARRIER_SYNC_NONE ,D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_RENDER_TARGET });
    forwardColoringPass.AddTextureInput(spotLightDepthBuffer, { D3D12_BARRIER_SYNC_PIXEL_SHADING ,D3D12_BARRIER_ACCESS_SHADER_RESOURCE, D3D12_BARRIER_LAYOUT_SHADER_RESOURCE });
    forwardColoringPass.AddTextureOutput(spotLightDepthBuffer, { D3D12_BARRIER_SYNC_NONE ,D3D12_BARRIER_ACCESS_NO_ACCESS, D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE });
}

void Renderer::ApplyPassBarriers(RenderGraph& renderGraph, PassType passType, ID3D12GraphicsCommandList7* pCommandList)
{
    std::vector<D3D12_BUFFER_BARRIER> bufferBarriers;
    std::vector<D3D12_TEXTURE_BARRIER> textureBarriers;

    for (const auto& barrier : renderGraph.GetCompiledBufferBarriers(passType))
    {
        auto pResources = renderGraph.GetResources(barrier.buffer, m_frameIndex);
        for (const auto& pResource : pResources)
        {
            D3D12_BUFFER_BARRIER b = {
                barrier.before.sync,
                barrier.after.sync,
                barrier.before.access,
                barrier.after.access,
                pResource,
                0,
                UINT64_MAX
            };
            bufferBarriers.push_back(b);
        }
    }

    for (const auto& barrier : renderGraph.GetCompiledTextureBarrier(passType))
    {
        auto pResources = renderGraph.GetResources(barrier.texture, m_frameIndex);
        for (const auto& pResource : pResources)
        {
            D3D12_TEXTURE_BARRIER b = {
                barrier.before.sync,
                barrier.after.sync,
                barrier.before.access,
                barrier.after.access,
                barrier.before.layout,
                barrier.after.layout,
                pResource,
                barrier.subresourceRange,
                D3D12_TEXTURE_BARRIER_FLAG_NONE
            };
            textureBarriers.push_back(b);
        }
    }

    std::vector<D3D12_BARRIER_GROUP> barrierGroups;
    if (!bufferBarriers.empty()) barrierGroups.push_back(BufferBarrierGroup(static_cast<UINT32>(bufferBarriers.size()), bufferBarriers.data()));
    if (!textureBarriers.empty()) barrierGroups.push_back(TextureBarrierGroup(static_cast<UINT32>(textureBarriers.size()), textureBarriers.data()));

    if (!barrierGroups.empty())
        pCommandList->Barrier(static_cast<UINT32>(barrierGroups.size()), barrierGroups.data());
}

void Renderer::SetTextureFiltering(TextureFiltering filtering)
{
    m_currentTextureFiltering = filtering;
    for (auto& material : m_sceneManager.GetMaterials())
    {
        material.BuildSamplerIndices(m_currentTextureFiltering);
    }
}

// Allocate Material
MaterialHandle Renderer::CreateMaterial()
{
    return m_sceneManager.AddMaterial(m_device.Get(), m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount));
}

// Allocate & register Material
MaterialHandle Renderer::CreateMaterial(const AssetID& id)
{
    return m_sceneManager.AddMaterial(m_device.Get(), m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount), id);
}

MaterialHandle Renderer::CloneMaterial(MaterialHandle src)
{
    auto hDst = CreateMaterial();
    auto* pSrc = m_sceneManager.GetMaterial(src);
    auto* pDst = m_sceneManager.GetMaterial(hDst);
    pDst->CopyDataFrom(*pSrc);
    return hDst;
}

MeshHandle Renderer::CreateMesh(ID3D12GraphicsCommandList7* pCommandList, TransientUploadAllocator& allocator, const GeometryData& data)
{
    return m_sceneManager.AddMesh(m_device.Get(), pCommandList, allocator, data);
}

DirectionalLightHandle Renderer::CreateDirectionalLight()
{
    return m_sceneManager.AddDirectionalLight(
        m_device.Get(),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(MAX_CASCADES),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount),
        m_shadowMapResolution);
}

PointLightHandle Renderer::CreatePointLight()
{
    return m_sceneManager.AddPointLight(
        m_device.Get(),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(POINT_LIGHT_ARRAY_SIZE),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_RTV]->Allocate(POINT_LIGHT_ARRAY_SIZE),
        m_shadowMapResolution);
}

SpotLightHandle Renderer::CreateSpotLight()
{
    return m_sceneManager.AddSpotLight(
        m_device.Get(),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_DSV]->Allocate(SPOT_LIGHT_ARRAY_SIZE),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(),
        m_descriptorAllocators[D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV]->Allocate(FrameCount),
        m_shadowMapResolution);
}

TextureHandle Renderer::CreateTexture(
    ID3D12GraphicsCommandList7* pCommandList,
    DescriptorAllocation&& allocation,
    TransientUploadAllocator& uploadAllocator,
    const std::vector<UINT8>& textureSrc,
    UINT width,
    UINT height)
{
    return m_sceneManager.AddTexture(
        m_device.Get(),
        pCommandList,
        std::move(allocation),
        uploadAllocator,
        textureSrc,
        width,
        height);
}

TextureHandle Renderer::CreateTexture(
    ID3D12GraphicsCommandList7* pCommandList,
    DescriptorAllocation&& allocation,
    TransientUploadAllocator& uploadAllocator,
    const std::wstring& filePath,
    bool isSRGB,
    bool useBlockCompress,
    bool flipImage,
    bool isCubeMap)
{
    return m_sceneManager.AddTexture(
        m_device.Get(),
        pCommandList,
        std::move(allocation),
        uploadAllocator,
        filePath,
        isSRGB,
        useBlockCompress,
        flipImage,
        isCubeMap);
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
        psoDesc.pRootSignature = m_rootSignature->GetRootSignature();

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

    // Transforms
    static float rotationSpeed = 1.0f;  // unit : rad/s

    for (auto& entity : m_sceneManager.GetEntities())
    {
        if (!entity.transform.has_value()) continue;
        entity.transform->SnapshotState();
    }

    for (auto& handle : m_previewRotations)
    {
        auto* pEntity = m_sceneManager.Get(handle);
        if (pEntity == nullptr) continue;
        pEntity->transform->Apply(XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, rotationSpeed * fixedDtSec, 0.0f), XMFLOAT3());
    }
}

void Renderer::PrepareConstantData(float alpha)
{
    // Transforms
    for (auto& entity : m_sceneManager.GetEntities())
    {
        if (entity.parent.index == UINT_MAX && entity.parent.generation == 0)
        {
            XMMATRIX accumulated = XMMatrixIdentity();
            PrepareTransform(entity, accumulated, alpha);
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

    UINT idx = 0;
    for (auto& light : m_sceneManager.GetDirectionalLights())
    {
        PrepareDirectionalLight(light, cascadeSpheres);
        light.SetIdxInArray(idx);
        ++idx;
    }
    idx = 0;
    for (auto& light : m_sceneManager.GetPointLights())
    {
        PreparePointLight(light);
        light.SetIdxInArray(idx);
        ++idx;
    }
    idx = 0;
    for (auto& light : m_sceneManager.GetSpotLights())
    {
        PrepareSpotLight(light);
        light.SetIdxInArray(idx);
        ++idx;
    }
}

void Renderer::PrepareTransform(Entity& entity, XMMATRIX& accumulated, float alpha)
{
    if (!entity.transform.has_value()) return;

    entity.transform->UpdateLocalRenderState(alpha);
    XMMATRIX localRenderTransform = XMLoadFloat4x4(&entity.transform->GetLocalRenderTransform());

    XMMATRIX world = localRenderTransform * accumulated;
    entity.transform->SetWorldRenderTransform(world);

    for (auto& child : entity.children)
        PrepareTransform(*m_sceneManager.Get(child), world, alpha);
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
    m_cameraUploadAllocation = frameResource.PushConstantData(&m_cameraConstantData, sizeof(CameraConstantData));
    m_shadowUploadAllocation = frameResource.PushConstantData(&m_shadowConstantData, sizeof(ShadowConstantData));

    for (auto& mat : m_sceneManager.GetMaterials())
    {
        auto alloc = frameResource.PushConstantData(mat.GetConstantDataPtr(), sizeof(MaterialConstantData));
        CreateCBV(m_device.Get(), alloc.GPUPtr, sizeof(MaterialConstantData), mat.GetCBVHandle(m_frameIndex));
    }

    auto processLight = [&](Light& light, UINT arraySize)
        {
            for (UINT i = 0; i < arraySize; ++i)
            {
                auto alloc = frameResource.PushConstantData(light.GetCameraConstantDataPtr(i), sizeof(CameraConstantData));
                light.SetCameraUploadAllocation(i, alloc);
            }
            auto alloc = frameResource.PushConstantData(light.GetLightConstantDataPtr(), sizeof(LightConstantData));
            CreateCBV(m_device.Get(), alloc.GPUPtr, sizeof(LightConstantData), light.GetLightCBVHandle(m_frameIndex));
        };

    for (auto& light : m_sceneManager.GetDirectionalLights())
        processLight(light, GetRequiredArraySize(LightType::DIRECTIONAL));
    for (auto& light : m_sceneManager.GetPointLights())
        processLight(light, GetRequiredArraySize(LightType::POINT));
    for (auto& light : m_sceneManager.GetSpotLights())
        processLight(light, GetRequiredArraySize(LightType::SPOT));
}

void Renderer::DrawMesh(ID3D12GraphicsCommandList7* pCommandList, MeshHandle meshhandle, PassType passType, D3D12_GPU_VIRTUAL_ADDRESS instanceBufferBase)
{
    static const UINT instanceDataSize = static_cast<UINT>(sizeof(InstanceData));

    const auto& instanceRange = m_sceneManager.GetInstanceRange(meshhandle);

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
    instanceBufferView.BufferLocation = instanceBufferBase + instanceRange.offset;
    if (passType == PassType::GBUFFER) instanceBufferView.BufferLocation += instanceRange.forwardCount * sizeof(InstanceData);
    instanceBufferView.StrideInBytes = instanceDataSize;
    instanceBufferView.SizeInBytes = instanceDataSize * instanceCount;

    auto* pMesh = m_sceneManager.GetMesh(meshhandle);

    D3D12_VERTEX_BUFFER_VIEW pVertexBufferViews[] = { pMesh->GetVBV(), instanceBufferView };
    pCommandList->IASetVertexBuffers(0, 2, pVertexBufferViews);
    pCommandList->IASetIndexBuffer(&pMesh->GetIBV());

    pCommandList->DrawIndexedInstanced(pMesh->GetNumIndices(), instanceCount, 0, 0, 0);
}