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

    static void HideCursor();
    static void RestoreCursor();

    inline static HWND sm_hwnd = nullptr;
    inline static POINT sm_savedCursorPos;
    inline static bool sm_isCursorHidden = false;
    inline static bool sm_cursorPosValid = false;
};