#pragma once

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include <d3d12.h>

#include "SceneHandles.h"

class Renderer;
class SceneManager;
class ImGuiDescriptorAllocator;
struct ImGuiMultiSelectIO;

class EditorUI
{
public:
    EditorUI(const EditorUI&) = delete;
    EditorUI& operator=(const EditorUI&) = delete;
    EditorUI(EditorUI&&) = delete;
    EditorUI& operator=(EditorUI&&) = delete;

    EditorUI() = default;
    ~EditorUI() = default;

    void Init(
        ID3D12Device10* pDevice,
        ID3D12CommandQueue* pCommandQueue,
        int numFramesInFlight,
        ImGuiDescriptorAllocator* pDescriptorAllocator,
        float dpiScale,
        Renderer* pRenderer,
        SceneManager* pSceneManager);

    void BeginFrame();
    void BuildImGuiFrame();
    void PopulateCommandList(ID3D12GraphicsCommandList* pCommandList);

    void RenderEntityNode(const Entity& entity, bool& del, std::vector<EntityHandle>& visibleOrder);
    void ApplySelectionRequests(ImGuiMultiSelectIO* ms, const std::vector<EntityHandle>& visibleOrder);

    void ClearSelection();
    void SelectSingle(EntityHandle handle);
    void ToggleSelect(EntityHandle handle);

    void SetDpiScale(float value);

    const std::unordered_set<EntityHandle>& GetSelection() const;

    void Destroy();

private:
    std::unordered_set<EntityHandle> m_selected;
    bool m_selectionChanged = false;

    std::string m_imguiIniPath; // UTF-8
    bool m_resetLayout = false;

    ID3D12Device10* m_pDevice = nullptr;
    ID3D12DescriptorHeap* m_pHeap = nullptr;
    Renderer* m_pRenderer = nullptr;
    SceneManager* m_pSceneManager = nullptr;

    UINT m_pendingSceneWidth = 0;
    UINT m_pendingSceneHeight = 0;
    std::chrono::time_point<std::chrono::steady_clock> m_lastResizeRequestTime;
};
