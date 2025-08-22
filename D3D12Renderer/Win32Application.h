#pragma once

#include <Windows.h>
#include "Renderer.h"

class Win32Application
{
public:
    static int Run(Renderer* pSample, HINSTANCE hInstance, int nCmdShow);
    static HWND GetHwnd() { return m_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    inline static HWND m_hwnd = nullptr;
};