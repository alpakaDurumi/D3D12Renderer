#include "pch.h"
#include "Win32Application.h"

#include <locale>

#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <timeapi.h>

#include "Renderer.h"

int Win32Application::Run(Renderer* pRenderer, HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    ParseCommandLineArgs(pRenderer, lpCmdLine);
    pRenderer->UpdateWidthHeight();

    // Set locale for converting between multi-byte character and wide character (e.g. std::mbstowcs)
    std::setlocale(LC_ALL, ".UTF8");

    // Register the window class
    WNDCLASSEXW windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = hInstance;
    windowClass.hIcon = nullptr;
    windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
    windowClass.hbrBackground = nullptr;
    windowClass.lpszMenuName = nullptr;
    windowClass.lpszClassName = L"DefaultWindowClass";
    windowClass.hIconSm = nullptr;
    if (RegisterClassExW(&windowClass) == 0)
    {
        MessageBoxW(nullptr, L"Window Class Registration Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Make application DPI-aware before adjusting window.
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    RECT windowRect = { 0, 0, static_cast<LONG>(pRenderer->GetWidth()), static_cast<LONG>(pRenderer->GetHeight()) };
    UINT dpi = GetDpiForSystem();
    AdjustWindowRectExForDpi(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi);

    // Create the window and store a handle to it.
    sm_hwnd = CreateWindowExW(
        NULL,
        windowClass.lpszClassName,
        pRenderer->GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        pRenderer);
    if (sm_hwnd == 0)
    {
        MessageBoxW(nullptr, L"Window Creation Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    pRenderer->OnInit(dpi);

    ShowWindow(sm_hwnd, nCmdShow);

    // Set periodic timer resolution.
    MMRESULT mm = timeBeginPeriod(1);
    bool timerResolutionSet = (mm == TIMERR_NOERROR);
    if (!timerResolutionSet)
    {
        OutputDebugStringW(L"timeBeginPeriod(1) failed\n");
    }

    // main loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        pRenderer->BuildImGuiFrame();
        pRenderer->OnUpdate();
        pRenderer->OnRender();
    }

    pRenderer->OnDestroy();

    // Unset periodic timer resolution.
    if (timerResolutionSet)
    {
        if (timeEndPeriod(1) == TIMERR_NOCANDO)
        {
            OutputDebugStringW(L"timeEndPeriod(1) failed\n");
        }
    }

    // return exit code that delivered by PostQuitMessage()
    return static_cast<int>(msg.wParam);
}

// Forward declaration
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK Win32Application::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
        return true;

    Renderer* renderer = reinterpret_cast<Renderer*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        // Save the Renderer* passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;
    case WM_SYSKEYDOWN:
        if (renderer)
        {
            // ALT + ENTER. Only if prev key is up
            if (wParam == VK_RETURN && lParam & (1 << 29) && !(lParam & (1 << 30)))
            {
                renderer->ToggleFullScreen();
            }
        }
        return 0;
    case WM_KEYDOWN:
        if (renderer)
        {
            // Only if prev key is up
            if (!(lParam & (1 << 30)))
            {
                renderer->OnKeyDown(wParam);
            }
        }
        return 0;
    case WM_KEYUP:
        if (renderer)
        {
            renderer->OnKeyUp(wParam);
        }
        return 0;
    case WM_LBUTTONDOWN:
        if (renderer)
        {
            renderer->OnMouseButtonDown(0);
        }
        return 0;
    case WM_LBUTTONUP:
        if (renderer)
        {
            renderer->OnMouseButtonUp(0);
        }
        return 0;
    case WM_RBUTTONDOWN:
        if (renderer)
        {
            renderer->OnMouseButtonDown(1);
        }
        return 0;
    case WM_RBUTTONUP:
        if (renderer)
        {
            renderer->OnMouseButtonUp(1);
        }
        return 0;
    case WM_MBUTTONDOWN:
        if (renderer)
        {
            renderer->OnMouseButtonDown(2);
        }
        return 0;
    case WM_MBUTTONUP:
        if (renderer)
        {
            renderer->OnMouseButtonUp(2);
        }
        return 0;
    case WM_MOUSEMOVE:
        if (renderer)
        {
            int xPos = LOWORD(lParam);
            int yPos = HIWORD(lParam);
            renderer->OnMouseMove(xPos, yPos);
        }
        return 0;
    case WM_SIZE:
        if (renderer)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            renderer->OnResize(width, height);
        }
        return 0;
    case WM_DPICHANGED:
        if (renderer)
        {
            UINT dpi = LOWORD(wParam);
            RECT* const newRect = (RECT*)lParam;
            SetWindowPos(sm_hwnd, HWND_TOP,
                newRect->left,
                newRect->top,
                newRect->right - newRect->left,
                newRect->bottom - newRect->top,
                SWP_NOZORDER | SWP_NOACTIVATE);
            renderer->OnDpiChanged(dpi);
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hWnd, message, wParam, lParam);
}

void Win32Application::ParseCommandLineArgs(Renderer* pRenderer, LPWSTR lpCmdLine)
{
    int argc;
    WCHAR** argv = CommandLineToArgvW(lpCmdLine, &argc);

    for (int i = 0; i < argc; i++)
    {
        if (wcscmp(argv[i], L"-w") == 0 || wcscmp(argv[i], L"--width") == 0)
            pRenderer->SetWidth(wcstol(argv[++i], nullptr, 10));
        if (wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--height") == 0)
            pRenderer->SetHeight(wcstol(argv[++i], nullptr, 10));
        if (wcscmp(argv[i], L"-warp") == 0 || wcscmp(argv[i], L"--warp") == 0)
            pRenderer->SetWarp(true);
        if (wcscmp(argv[i], L"-pix") == 0 || wcscmp(argv[i], L"--pix") == 0)
            pRenderer->SetPix();
    }

    LocalFree(argv);
}