#include <Windows.h>
#include "Win32Application.h"

extern "C" { __declspec(dllexport) extern const UINT D3D12SDKVersion = 717; }
extern "C" { __declspec(dllexport) extern const char* D3D12SDKPath = u8".\\D3D12\\"; }

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    Renderer renderer(L"D3D12 Renderer");
    return Win32Application::Run(&renderer, hInstance, lpCmdLine, nShowCmd);
}