#pragma once

#include <chrono>
#include <string>
#include <unordered_set>
#include <vector>

#include <d3d12.h>

#include "ImGuiDescriptorAllocator.h"
#include "ImGuiShaderResourceView.h"
#include "SceneHandles.h"

class Renderer;
class SceneManager;
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
        ID3D12Device* pDevice,
        ID3D12CommandQueue* pCommandQueue,
        const std::vector<ID3D12Resource*>& pToneMappedBuffers,
        float dpiScale,
        Renderer* pRenderer,
        SceneManager* pSceneManager);

    void BeginFrame();
    void BuildImGuiFrame(UINT frameIndex);
    void PopulateCommandList(ID3D12GraphicsCommandList* pCommandList);
    void UpdateToneMappedBuffersSrvs(ID3D12Device* pDevice, const std::vector<ID3D12Resource*>& pToneMappedBuffers);
    void SetDpiScale(float value);
    const std::unordered_set<EntityHandle>& GetSelection() const;
    void Destroy();

private:
    void RenderEntityNode(const Entity& entity, bool& del, std::vector<EntityHandle>& visibleOrder);
    void ApplySelectionRequests(ImGuiMultiSelectIO* ms, const std::vector<EntityHandle>& visibleOrder);
    void ClearSelection();
    void SelectSingle(EntityHandle handle);
    void ToggleSelect(EntityHandle handle);

    std::unordered_set<EntityHandle> m_selected;
    bool m_selectionChanged = false;

    std::string m_imguiIniPath; // UTF-8
    bool m_resetLayout = false;

    Renderer* m_pRenderer = nullptr;
    SceneManager* m_pSceneManager = nullptr;

    UINT m_pendingSceneWidth = 0;
    UINT m_pendingSceneHeight = 0;
    std::chrono::time_point<std::chrono::steady_clock> m_lastResizeRequestTime;

    ImGuiDescriptorAllocator m_imguiDescriptorAllocator;
    std::vector<ImGuiShaderResourceView> m_toneMappedBufferSrvs;
};
