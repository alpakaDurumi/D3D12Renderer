#include "Win32Application.h"

int Win32Application::Run(Renderer* pRenderer, HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    ParseCommandLineArgs(pRenderer, lpCmdLine);
    pRenderer->UpdateViewport();

    // Register the window class
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"DefaultWindowClass";
    if (RegisterClassEx(&windowClass) == 0)
    {
        MessageBox(nullptr, L"Window Class Registration Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    RECT windowRect = { 0, 0, static_cast<LONG>(pRenderer->GetWidth()), static_cast<LONG>(pRenderer->GetHeight()) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
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
    if (m_hwnd == 0)
    {
        MessageBox(nullptr, L"Window Creation Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    pRenderer->OnInit();

    ShowWindow(m_hwnd, nCmdShow);

    // main loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            pRenderer->OnUpdate();
            pRenderer->OnRender();
        }
    }

    pRenderer->OnDestroy();

    // return exit code that delivered by PostQuitMessage()
    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK Win32Application::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    Renderer* renderer = reinterpret_cast<Renderer*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
    {
        // Save the Renderer* passed in to CreateWindow.
        LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
    }
    return 0;
    case WM_KEYDOWN:
        if (renderer)
        {
            renderer->OnKeyDown(wParam);
        }
        return 0;
    case WM_KEYUP:
        if (renderer)
        {
            renderer->OnKeyUp(wParam);
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
    {
        if (renderer)
        {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            renderer->OnResize(width, height);
        }
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
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
    }

    LocalFree(argv);
}