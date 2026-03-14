#pragma once

#include <Windows.h>

class Renderer;

class Win32Application
{
public:
    static int Run(Renderer* pRenderer, HINSTANCE hInstance, LPWSTR lpCmdLine, int nCmdShow);
    static HWND GetHwnd() { return sm_hwnd; }

private:
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    static void ParseCommandLineArgs(Renderer* pRenderer, LPWSTR lpCmdLine);
    inline static HWND sm_hwnd = nullptr;
};