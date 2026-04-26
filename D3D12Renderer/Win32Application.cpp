#include "pch.h"
#include "Win32Application.h"

#include <windowsx.h>
#include <hidusage.h>

#include <locale>

#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <timeapi.h>

#include "Renderer.h"
#include "Aliases.h"

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
    sm_dpi = GetDpiForSystem();       // Before creating window, get DPI from system
    AdjustWindowRectExForDpi(&windowRect, WS_OVERLAPPEDWINDOW, FALSE, 0, sm_dpi);

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

    // Replace DPI with fresh value
    sm_dpi = GetDpiForWindow(sm_hwnd);

    // Register Raw Input Devices
    RAWINPUTDEVICE devices[2];

    devices[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
    devices[0].usUsage = HID_USAGE_GENERIC_MOUSE;
    devices[0].dwFlags = RIDEV_INPUTSINK;
    devices[0].hwndTarget = sm_hwnd;

    devices[1].usUsagePage = HID_USAGE_PAGE_GENERIC;
    devices[1].usUsage = HID_USAGE_GENERIC_KEYBOARD;
    devices[1].dwFlags = 0;
    devices[1].hwndTarget = sm_hwnd;

    RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE));

    pRenderer->OnInit(sm_dpi);

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

    switch (message)
    {
    case WM_CREATE:
    {
        // Save the Renderer* passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
        return 0;
    }
    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    }
    }

    Renderer* renderer = reinterpret_cast<Renderer*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    if (!renderer) return DefWindowProcW(hWnd, message, wParam, lParam);

    switch (message)
    {
    case WM_INPUT:
    {
        HandleRawInput(renderer, lParam);
        break;      // To call DefWindowProcW so the system can perform cleanup
    }
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        VKCode key = static_cast<VKCode>(wParam);
        WORD keyFlags = HIWORD(lParam);

        switch (message)
        {
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            // Only if prev key is up
            if (!(keyFlags & KF_REPEAT))
                renderer->OnKeyDown(key);
            return 0;
        case WM_KEYUP:
        case WM_SYSKEYUP:
            renderer->OnKeyUp(key);
            return 0;
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
        renderer->OnMouseButtonDown(0);
        return 0;
    case WM_LBUTTONUP:
        renderer->OnMouseButtonUp(0);
        return 0;
    case WM_RBUTTONDOWN:
        renderer->OnMouseButtonDown(1);
        HideCursor();
        GetCursorPos(&sm_savedCursorPos);
        sm_cursorPosValid = true;
        return 0;
    case WM_RBUTTONUP:
        if (sm_cursorPosValid) SetCursorPos(sm_savedCursorPos.x, sm_savedCursorPos.y);
        sm_cursorPosValid = false;
        RestoreCursor();
        renderer->OnMouseButtonUp(1);
        return 0;
    case WM_MBUTTONDOWN:
        renderer->OnMouseButtonDown(2);
        return 0;
    case WM_MBUTTONUP:
        renderer->OnMouseButtonUp(2);
        return 0;
    case WM_KILLFOCUS:
        sm_cursorPosValid = false;
        RestoreCursor();
        renderer->OnKillFocus();
        return 0;
    case WM_SIZE:
    {
        UINT width = LOWORD(lParam);
        UINT height = HIWORD(lParam);
        renderer->OnResize(width, height);
        return 0;
    }
    case WM_DPICHANGED:
    {
        sm_dpi = LOWORD(wParam);

        RECT* const newRect = reinterpret_cast<RECT*>(lParam);
        SetWindowPos(hWnd, HWND_TOP,
            newRect->left,
            newRect->top,
            newRect->right - newRect->left,
            newRect->bottom - newRect->top,
            SWP_NOZORDER | SWP_NOACTIVATE);
        renderer->OnDpiChanged(sm_dpi);
        return 0;
    }
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

void Win32Application::HideCursor()
{
    if (!sm_isCursorHidden)
    {
        ShowCursor(FALSE);
        sm_isCursorHidden = true;
    }
}

void Win32Application::RestoreCursor()
{
    if (sm_isCursorHidden)
    {
        ShowCursor(TRUE);
        sm_isCursorHidden = false;
    }
}

void Win32Application::HandleRawInput(Renderer* pRenderer, LPARAM lParam)
{
    UINT size = 0;

    // Get required size first
    GetRawInputData(
        reinterpret_cast<HRAWINPUT>(lParam),
        RID_INPUT,
        nullptr,
        &size,
        sizeof(RAWINPUTHEADER));

    UINT8 buffer[256];
    if (size > sizeof(buffer))
        return;

    // Get actual data
    GetRawInputData(
        reinterpret_cast<HRAWINPUT>(lParam),
        RID_INPUT,
        buffer,
        &size,
        sizeof(RAWINPUTHEADER));

    RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(buffer);

    if (raw->header.dwType == RIM_TYPEMOUSE)
    {
        const RAWMOUSE& mouse = raw->data.mouse;

        // Get cursor position
        POINT curPos;
        GetCursorPos(&curPos);
        ScreenToClient(sm_hwnd, &curPos);

        int dx = 0;
        int dy = 0;

        if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
        {
            if (sm_lastCursorPosValid)
            {
                dx = curPos.x - sm_lastCursorPos.x;
                dy = curPos.y - sm_lastCursorPos.y;
                pRenderer->OnMouseMove(dx, dy, curPos.x, curPos.y);
            }
        }
        else
        {
            dx = static_cast<int>(mouse.lLastX);
            dy = static_cast<int>(mouse.lLastY);
            pRenderer->OnMouseMove(dx, dy, curPos.x, curPos.y);
        }

        sm_lastCursorPos = curPos;
        sm_lastCursorPosValid = false;

        if (mouse.usButtonFlags & RI_MOUSE_WHEEL)
        {
            short wheelDelta = static_cast<short>(mouse.usButtonData);
            float stepDelta = static_cast<float>(wheelDelta) / WHEEL_DELTA;
            pRenderer->OnMouseWheel(stepDelta);
        }
    }
    else    // TODO: Keyboard
    {
    }
}