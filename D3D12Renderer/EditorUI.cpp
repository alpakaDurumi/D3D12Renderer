#include "pch.h"

#include "EditorUI.h"

#include <filesystem>

#include <DirectXMath.h>
#include <shlobj.h>

#include <imgui.h>
#include <imgui_impl_dx12.h>
#include <imgui_impl_win32.h>
#include <imgui_internal.h>

#include "D3DHelper.h"
#include "Renderer.h"
#include "RendererConfig.h"
#include "SceneManager.h"
#include "Win32Application.h"

using namespace DirectX;

// Pack an EntityHandle into the 64-bit value ImGui echoes back in selection requests
static ImGuiSelectionUserData ToSelectionUserData(EntityHandle h)
{
    return static_cast<ImGuiSelectionUserData>((static_cast<UINT64>(h.index) << 32) | h.generation);
}

// Unpack a value from a selection request back into an EntityHandle
static EntityHandle FromSelectionUserData(ImGuiSelectionUserData v)
{
    const UINT64 u = static_cast<UINT64>(v);
    return EntityHandle{static_cast<UINT>(u >> 32), static_cast<UINT>(u)};
}

void EditorUI::Init(
    ID3D12Device* pDevice,
    ID3D12CommandQueue* pCommandQueue,
    const std::vector<ID3D12Resource*>& pToneMappedBuffers,
    float dpiScale,
    Renderer* pRenderer,
    SceneManager* pSceneManager)
{
    m_imguiDescriptorAllocator.Init(pDevice);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

    ImGui_ImplWin32_Init(Win32Application::GetHwnd());

    // Setup Platform/Renderer backends
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = pDevice;
    init_info.CommandQueue = pCommandQueue;
    init_info.NumFramesInFlight = FrameCount;
    init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    init_info.DSVFormat = DXGI_FORMAT_UNKNOWN;
    init_info.UserData = &m_imguiDescriptorAllocator;
    init_info.SrvDescriptorHeap = m_imguiDescriptorAllocator.GetDescriptorHeap();
    // set callback functions for ImGui SRV descriptor
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
        { return static_cast<ImGuiDescriptorAllocator*>(info->UserData)->Allocate(out_cpu_handle, out_gpu_handle); };
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
        { return static_cast<ImGuiDescriptorAllocator*>(info->UserData)->Free(cpu_handle, gpu_handle); };
    ImGui_ImplDX12_Init(&init_info);

    ImGui::GetStyle().FontScaleMain = dpiScale;

    // Create config directory in LocalAppData if not exists
    PWSTR localAppDataPath = nullptr;
    SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &localAppDataPath);

    std::filesystem::path configPath = std::filesystem::path(localAppDataPath) / "D3D12Renderer";
    CoTaskMemFree(localAppDataPath);

    bool configPathReady = false;
    if (CreateDirectoryW(configPath.c_str(), nullptr))
        configPathReady = true;
    else
    {
        DWORD attr = GetFileAttributesW(configPath.c_str());
        configPathReady = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_DIRECTORY);
    }

    // Set ImGui ini file in LocalAppData
    if (configPathReady)
    {
        m_imguiIniPath = (configPath / "imgui.ini").u8string();
        io.IniFilename = m_imguiIniPath.c_str();
    }
    // else: use ImGui default setting ("imgui.ini" in CWD)

    m_pRenderer = pRenderer;
    m_pSceneManager = pSceneManager;

    // init toneMappedBufferSrvs
    m_toneMappedBufferSrvs.resize(FrameCount);
    for (UINT i = 0; i < FrameCount; i++)
        m_toneMappedBufferSrvs[i] = ImGuiShaderResourceView(m_imguiDescriptorAllocator.Allocate());
    UpdateToneMappedBuffersSrvs(pDevice, pToneMappedBuffers);
}

// Start the Dear ImGui frame
void EditorUI::BeginFrame()
{
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void EditorUI::ProcessInput()
{
    XMINT2 mouseMove = m_mouseDelta;
    m_mouseDelta = {0, 0};

    // ESC
    if (ImGui::Shortcut(ImGuiKey_Escape, ImGuiInputFlags_RouteGlobal))
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

    if (ImGui::Shortcut(ImGuiKey_F11, ImGuiInputFlags_RouteGlobal) || ImGui::Shortcut(ImGuiMod_Alt | ImGuiKey_Enter, ImGuiInputFlags_RouteGlobal))
        m_pRenderer->ToggleFullScreen();

    if (ImGui::Shortcut(ImGuiKey_V, ImGuiInputFlags_RouteGlobal))
        m_pRenderer->SetVSync(!m_pRenderer->GetVSync());

    // Focus
    if (ImGui::Shortcut(ImGuiKey_F, ImGuiInputFlags_RouteGlobal) && !m_selected.empty())
    {
        XMVECTOR acc = XMVectorZero();

        for (const auto& handle : m_selected)
        {
            XMFLOAT4X4 world = m_pSceneManager->Get(handle)->transform.GetWorldRenderTransform();
            acc += XMVectorSet(world._41, world._42, world._43, 0.0f);
        }

        XMVECTOR center = XMVectorScale(acc, 1.0f / static_cast<float>(m_selected.size()));
        m_camera.SetPosition(center - m_camera.GetForward() * DEFAULT_FOCUS_DIST);

        XMStoreFloat3(&m_orbitPivot, center);
        m_orbitDistance = DEFAULT_FOCUS_DIST;
    }

    // Dolly
    {
        static float cameraDollySpeed = 5.0f;

        float wheelStep = ImGui::GetIO().MouseWheel;
        if (wheelStep != 0.0f)
        {
            m_camera.MoveForward(wheelStep * cameraDollySpeed);
            XMVECTOR camPos = m_camera.GetPosition();
            m_orbitDistance = XMVectorGetX(XMVector3Length(camPos - XMLoadFloat3(&m_orbitPivot)));
        }
    }

    // Camera control
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        m_cameraControl = true;
        Win32Application::HideCursor();
    }
    if (m_cameraControl)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            m_camera.Rotate(mouseMove);
        }
        else
        {
            m_cameraControl = false;
            Win32Application::RestoreCursor();
        }
    }

    // Orbit
    if (ImGui::GetIO().KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        XMStoreFloat3(&m_orbitPivot, m_camera.GetPosition() + m_camera.GetForward() * m_orbitDistance);
        m_orbiting = true;
        Win32Application::HideCursor();
    }
    if (m_orbiting)
    {
        if (ImGui::GetIO().KeyAlt && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            m_camera.Orbit(XMLoadFloat3(&m_orbitPivot), m_orbitDistance, mouseMove);
        }
        else
        {
            m_orbiting = false;
            Win32Application::RestoreCursor();
        }
    }

    // Pan
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
    {
        m_panning = true;
        Win32Application::HideCursor();
    }
    if (m_panning)
    {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        {
            m_camera.Pan(mouseMove);
        }
        else
        {
            m_panning = false;
            Win32Application::RestoreCursor();
        }
    }

    // Move
    static float cameraMoveSpeed = 50.0f;
    // Cap the frame delta used for camera movement. Without the cap, a long frame (e.g. a hitch
    // while flying) moves the camera a large distance in a single frame.
    // 1.0 s is the upper bound used by Unreal's FEditorViewportClient (EditorMovementDeltaUpperBound).
    static constexpr float maxCameraDtSec = 1.0f;
    const float frameDtSec = std::min(m_pRenderer->GetDeltaTime().count() * 1e-9f, maxCameraDtSec);
    float dist = cameraMoveSpeed * frameDtSec;
    if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
    {
        if (ImGui::IsKeyDown(ImGuiKey_W))
            m_camera.MoveForward(dist);
        if (ImGui::IsKeyDown(ImGuiKey_A))
            m_camera.MoveRight(-dist);
        if (ImGui::IsKeyDown(ImGuiKey_S))
            m_camera.MoveForward(-dist);
        if (ImGui::IsKeyDown(ImGuiKey_D))
            m_camera.MoveRight(dist);
        if (ImGui::IsKeyDown(ImGuiKey_Q))
            m_camera.MoveUp(-dist);
        if (ImGui::IsKeyDown(ImGuiKey_E))
            m_camera.MoveUp(dist);
    }
}

void EditorUI::BuildImGuiFrame(UINT frameIndex)
{
    static UINT64 frameCounter = 0;
    static std::chrono::nanoseconds elapsed = std::chrono::nanoseconds::zero();
    static double fps = 0.0;
    static double frameTime = 0.0;

    // Menu
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("Add"))
        {
            if (ImGui::MenuItem("Cube"))
                m_pRenderer->SpawnPrimitive("builtin://mesh/cube", "New Cube");
            if (ImGui::MenuItem("Sphere"))
                m_pRenderer->SpawnPrimitive("builtin://mesh/sphere", "New Sphere");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    // ImGuiID string is hashed and stored in the INI file.
    // Changing it will invalidate any previously saved settings associated with it.
    // Also, GetID uses Window ID Stack as seed.
    ImGuiID dockSpaceId = ImGui::GetID("My Dockspace");
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    // Use DockBuilder API to set layout.
    // if the INI file does not contain DockSpaceId information or if a layout reset has been requested.
    if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr || m_resetLayout)
    {
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->Size);

        ImGuiID leftId = 0;
        ImGuiID centerId = 0;
        ImGui::DockBuilderSplitNode(dockSpaceId, ImGuiDir_Left, 0.20f, &leftId, &centerId);

        ImGuiID leftTopId = 0;
        ImGuiID leftBottomId = 0;
        ImGui::DockBuilderSplitNode(leftId, ImGuiDir_Up, 0.50f, &leftTopId, &leftBottomId);

        ImGuiID rightId = 0;
        ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, 0.20f, &rightId, &centerId);

        ImGui::DockBuilderDockWindow("Scene", centerId);
        ImGui::DockBuilderDockWindow("Test", leftTopId);
        ImGui::DockBuilderDockWindow("Hierarchy", leftBottomId);
        ImGui::DockBuilderDockWindow("Inspector", rightId);

        ImGui::DockBuilderFinish(dockSpaceId);

        m_resetLayout = false;
    }

    ImGui::DockSpaceOverViewport(dockSpaceId, viewport, ImGuiDockNodeFlags_None);

    // Scene window
    {
        ImGui::Begin("Scene");

        static constexpr double DEBOUNCE_DELAY = 0.15; // 0.15 sec

        ImVec2 measured = ImGui::GetContentRegionAvail();

        if (measured.x < 1.0f || measured.y < 1.0f)
        {
            m_sceneHovered = false;
            m_sceneActive = false;
        }
        else
        {
            const UINT width = static_cast<UINT>(measured.x);
            const UINT height = static_cast<UINT>(measured.y);

            const auto now = m_pRenderer->GetCurrentTimePoint();

            // If scene size changed
            if (width != m_pendingSceneWidth || height != m_pendingSceneHeight)
            {
                m_pendingSceneWidth = width;
                m_pendingSceneHeight = height;
                m_lastResizeRequestTime = now;
            }
            // If debounce delay has passed
            else if (std::chrono::duration<double>(now - m_lastResizeRequestTime).count() >= DEBOUNCE_DELAY)
            {
                m_pRenderer->ResizeSceneResolution(width, height);
                m_camera.SetAspectRatio(static_cast<float>(width) / static_cast<float>(height));
            }

            ImGui::InvisibleButton("SceneViewport", measured, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

            const ImVec2 rectMin = ImGui::GetItemRectMin();
            const ImVec2 rectMax = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddImage(static_cast<ImTextureID>(m_toneMappedBufferSrvs[frameIndex].GetGpuHandle().ptr), rectMin, rectMax);

            m_sceneHovered = ImGui::IsItemHovered();
            m_sceneActive = ImGui::IsItemActive();

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::GetIO().KeyAlt)
                ClearSelection();
        }

        ImGui::End();
    }

    // Test window
    {
        ImGui::Begin("Test");

        ++frameCounter;

        elapsed += m_pRenderer->GetDeltaTime();
        const double elapsedSeconds = std::chrono::duration<double>(elapsed).count();
        if (elapsedSeconds >= 1.0)
        {
            fps = frameCounter / elapsedSeconds;
            frameTime = 1000.0 / fps;

            frameCounter = 0;
            elapsed = std::chrono::nanoseconds::zero();
        }

        ImGui::Text("FPS: %.1f", fps);
        ImGui::Text("Latency: %.3f", frameTime);

        bool vSync = m_pRenderer->GetVSync();
        if (ImGui::Checkbox("vSync", &vSync))
            m_pRenderer->SetVSync(vSync);

        const char* items0[] = {"Unlimited", "30", "60", "120", "144", "160", "240"};
        static int item0_selected_idx = 0;

        // FPS cap can be set when vSync enabled.
        ImGui::BeginDisabled(vSync);
        if (ImGui::BeginCombo("FPS Cap", items0[item0_selected_idx]))
        {
            for (int n = 0; n < IM_ARRAYSIZE(items0); ++n)
            {
                const bool is_selected = item0_selected_idx == n;
                if (ImGui::Selectable(items0[n], is_selected))
                {
                    item0_selected_idx = n;
                    m_pRenderer->SetFpsCap(std::string(items0[n]));
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();

        const char* items[] = {"Point", "Bilinear", "AnisotropicX2", "AnisotropicX4", "AnisotropicX8", "AnisotropicX16"};
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
                    m_pRenderer->SetTextureFiltering(static_cast<TextureFiltering>(n));
                }

                // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                if (is_selected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        if (ImGui::Button("Reset Layout"))
        {
            m_resetLayout = true;
        }

        ImGui::Text("Visible Count: %u", m_pRenderer->GetVisibleCount());

        ImGui::End();
    }

    // Hierarchy window
    {
        ImGui::Begin("Hierarchy");

        bool del = !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete);

        std::vector<EntityHandle> visibleOrder;
        ImGuiMultiSelectIO* ms = ImGui::BeginMultiSelect(ImGuiMultiSelectFlags_ClearOnClickVoid, static_cast<int>(m_selected.size()));
        ApplySelectionRequests(ms, visibleOrder);

        for (const auto& entity : m_pSceneManager->GetEntities())
            if (entity.parent.Empty())
                RenderEntityNode(entity, del, visibleOrder);

        ms = ImGui::EndMultiSelect();
        ApplySelectionRequests(ms, visibleOrder);

        if (del)
        {
            for (const auto& handle : m_selected)
                m_pSceneManager->Remove(handle);
            ClearSelection();
        }

        ImGui::End();
    }

    // Inspector
    {
        ImGui::Begin("Inspector");

        if (!m_selected.empty())
        {
            // Transform component
            // Drag editing is available only when a single entity is selected.
            // On multi-selection every component becomes a text box that parses the typed value,
            // even for components whose values are identical across the selection.

            // get: Transform& -> XMFLOAT3
            // set: (Transform&, const XMFLOAT3&) -> void
            auto drawTransform = [&](const char* label, auto&& get, auto&& set)
            {
                auto it = m_selected.begin();
                XMFLOAT3 common = get(m_pSceneManager->Get(*it)->transform);

                if (m_selected.size() == 1)
                {
                    XMFLOAT3 v = common;
                    if (ImGui::DragFloat3(label, &v.x))
                        set(m_pSceneManager->Get(*m_selected.begin())->transform, v);
                    return;
                }

                bool mixed[3] = {false};
                for (++it; it != m_selected.end(); ++it)
                {
                    XMFLOAT3 o = get(m_pSceneManager->Get(*it)->transform);
                    if (o.x != common.x) mixed[0] = true;
                    if (o.y != common.y) mixed[1] = true;
                    if (o.z != common.z) mixed[2] = true;
                }

                // Multi-selection: one text box per component
                ImGui::BeginGroup();
                ImGui::PushID(label);
                ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());

                for (int i = 0; i < 3; ++i)
                {
                    ImGui::PushID(i);
                    if (i > 0)
                        ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);

                    char buf[32] = "";
                    if (mixed[i])
                        snprintf(buf, sizeof(buf), "Multiple Values");
                    else
                        snprintf(buf, sizeof(buf), "%.3f", (&common.x)[i]);

                    if (ImGui::InputText("", buf, sizeof(buf), ImGuiInputTextFlags_CharsScientific | ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                    {
                        char* end = nullptr;
                        float v = std::strtof(buf, &end);

                        if (end != buf)
                        {
                            for (const auto& handle : m_selected)
                            {
                                auto& tr = m_pSceneManager->Get(handle)->transform;
                                XMFLOAT3 t = get(tr);
                                (&t.x)[i] = v;
                                set(tr, t);
                            }
                        }
                    }

                    ImGui::PopID();
                    ImGui::PopItemWidth();
                }

                ImGui::PopID();
                ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
                ImGui::TextUnformatted(label);
                ImGui::EndGroup();
            };

            drawTransform("Scale", [](Transform& tr)
                          { return tr.GetScale(); }, [](Transform& tr, const XMFLOAT3& v)
                          { tr.SetScale(v); });

            drawTransform("Rotation", [&](Transform& tr)
                          { return tr.GetEulerCache(m_selectionChanged); }, [](Transform& tr, const XMFLOAT3& v)
                          { tr.SetRotation(v); });

            drawTransform("Translation", [](Transform& tr)
                          { return tr.GetTranslation(); }, [](Transform& tr, const XMFLOAT3& v)
                          { tr.SetTranslation(v); });

            // Light component
            bool allHaveLight = true;
            for (const auto& handle : m_selected)
            {
                if (!m_pSceneManager->Get(handle)->light.has_value())
                {
                    allHaveLight = false;
                    break;
                }
            }

            if (allHaveLight)
            {
                auto getResolution = [&](EntityHandle handle)
                {
                    return m_pSceneManager->GetShadowMapResolution(m_pSceneManager->Get(handle)->light.value());
                };

                // Take the first value as representative and check whether the selection is mixed
                auto it = m_selected.begin();
                UINT common = getResolution(*it);
                bool mixed = false;
                for (++it; it != m_selected.end(); ++it)
                {
                    if (getResolution(*it) != common)
                    {
                        mixed = true;
                        break;
                    }
                }

                char buf[16];
                if (mixed)
                    snprintf(buf, sizeof(buf), "Multiple Values");
                else
                    snprintf(buf, sizeof(buf), "%u", common);

                const char* items[] = {"512", "1024", "2048", "4096"};

                if (ImGui::BeginCombo("Shadow Map Resolution", buf))
                {
                    for (int n = 0; n < IM_ARRAYSIZE(items); ++n)
                    {
                        const UINT resolution = static_cast<UINT>(std::stoi(items[n]));
                        const bool isSelected = !mixed && resolution == common;
                        if (ImGui::Selectable(items[n], isSelected))
                            for (const auto& handle : m_selected)
                                m_pSceneManager->SetShadowMapResolution(m_pSceneManager->Get(handle)->light.value(), resolution);

                        // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
            }
        }
        ImGui::End();
    }
    m_selectionChanged = false;
}

void EditorUI::PopulateCommandList(ID3D12GraphicsCommandList* pCommandList)
{
    // End the ImGui frame and finalize the draw data
    ImGui::Render();

    // ImGui uses its dedicated descriptor heap for now, so calling SetDescriptorHeaps is mandatory
    ID3D12DescriptorHeap* ppHeaps[] = {m_imguiDescriptorAllocator.GetDescriptorHeap()};
    pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Populate commands for ImGui
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), pCommandList);
}

void EditorUI::UpdateToneMappedBuffersSrvs(ID3D12Device* pDevice, const std::vector<ID3D12Resource*>& pToneMappedBuffers)
{
    const auto format = DXGI_FORMAT_R8G8B8A8_UNORM;

    for (UINT i = 0; i < FrameCount; ++i)
        m_toneMappedBufferSrvs[i].Init(pDevice, pToneMappedBuffers[i], D3DHelper::GetSrvDesc(format, 1));
}

void EditorUI::SetDpiScale(float value)
{
    ImGui::GetStyle().FontScaleMain = value;
}

const std::unordered_set<EntityHandle>& EditorUI::GetSelection() const
{
    return m_selected;
}

const Camera& EditorUI::GetCamera() const
{
    return m_camera;
}

void EditorUI::AddMouseDelta(int dx, int dy)
{
    m_mouseDelta.x += dx;
    m_mouseDelta.y += dy;
}

void EditorUI::Destroy()
{
    // Shutdown ImGui
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void EditorUI::RenderEntityNode(const Entity& entity, bool& del, std::vector<EntityHandle>& visibleOrder)
{
    bool isSelected = m_selected.find(entity.selfHandle) != m_selected.end();

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
    if (entity.children.empty())
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (isSelected)
        flags |= ImGuiTreeNodeFlags_Selected;

    UINT64 id = (static_cast<UINT64>(entity.selfHandle.index) << 32) | entity.selfHandle.generation;

    visibleOrder.push_back(entity.selfHandle);
    ImGui::SetNextItemSelectionUserData(ToSelectionUserData(entity.selfHandle));
    bool isExpanded = ImGui::TreeNodeEx(reinterpret_cast<void*>(id), flags, "%s", entity.name.c_str());

    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::MenuItem("Delete"))
            del = true;
        ImGui::EndPopup();
    }
    if (!(flags & ImGuiTreeNodeFlags_NoTreePushOnOpen) && isExpanded)
    {
        for (auto c : entity.children)
            RenderEntityNode(*m_pSceneManager->Get(c), del, visibleOrder);
        ImGui::TreePop();
    }
}

// Apply ImGui multi-select requests to m_selected, resolving ranges through the visible tree order
void EditorUI::ApplySelectionRequests(ImGuiMultiSelectIO* ms, const std::vector<EntityHandle>& visibleOrder)
{
    if (ms->Requests.empty())
        return;

    const auto before = m_selected;

    for (const ImGuiSelectionRequest& req : ms->Requests)
    {
        if (req.Type == ImGuiSelectionRequestType_SetAll)
        {
            m_selected.clear();
            if (req.Selected)
                for (const auto& entity : m_pSceneManager->GetEntities())
                    m_selected.insert(entity.selfHandle);
        }
        else if (req.Type == ImGuiSelectionRequestType_SetRange)
        {
            auto first = std::find(visibleOrder.begin(), visibleOrder.end(), FromSelectionUserData(req.RangeFirstItem));
            auto last = std::find(visibleOrder.begin(), visibleOrder.end(), FromSelectionUserData(req.RangeLastItem));
            if (first == visibleOrder.end() || last == visibleOrder.end())
                continue;
            if (first > last)
                std::swap(first, last);

            for (auto it = first; it != std::next(last); ++it)
            {
                if (req.Selected)
                    m_selected.insert(*it);
                else
                    m_selected.erase(*it);
            }
        }
    }
    // TODO: The current logic for detecting changes is quite naive and can be optimized.
    if (m_selected != before)
        m_selectionChanged = true;
}

void EditorUI::ClearSelection()
{
    if (m_selected.empty())
        return;

    m_selected.clear();
    m_selectionChanged = true;
}

void EditorUI::SelectSingle(EntityHandle handle)
{
    if (m_selected.size() == 1 && *m_selected.begin() == handle)
        return;

    m_selected.clear();
    m_selected.insert(handle);
    m_selectionChanged = true;
}

void EditorUI::ToggleSelect(EntityHandle handle)
{
    if (!m_selected.insert(handle).second)
        m_selected.erase(handle);

    m_selectionChanged = true;
}
