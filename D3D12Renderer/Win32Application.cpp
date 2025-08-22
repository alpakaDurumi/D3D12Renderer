#include "Win32Application.h"

int Win32Application::Run(Renderer* renderer, HINSTANCE hInstance, int nCmdShow) {
    // Register the window class
    WNDCLASSEX windowClass = { 0 };
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = L"DefaultWindowClass";
    if (RegisterClassEx(&windowClass) == 0) {
        MessageBox(nullptr, L"Window Class Registration Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    RECT windowRect = { 0, 0, static_cast<LONG>(renderer->GetWidth()), static_cast<LONG>(renderer->GetHeight()) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

    // Create the window and store a handle to it.
    m_hwnd = CreateWindow(
        windowClass.lpszClassName,
        renderer->GetTitle(),
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,        // We have no parent window.
        nullptr,        // We aren't using menus.
        hInstance,
        renderer);
    if (m_hwnd == 0) {
        MessageBox(nullptr, L"Window Creation Failed!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    //renderer->OnInit();
    
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
    }

    //renderer->OnDestroy();

    // Return this part of the WM_QUIT message to Windows.
    return static_cast<char>(msg.wParam);
}

LRESULT CALLBACK Win32Application::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
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
    case WM_SIZE:
    {
        // resize
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}