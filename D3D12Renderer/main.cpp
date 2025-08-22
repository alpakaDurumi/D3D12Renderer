#include <Windows.h>
#include "Win32Application.h"

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nShowCmd)
{
    Renderer test(1280, 720, L"Window test");
    return Win32Application::Run(&test, hInstance, nShowCmd);
}
