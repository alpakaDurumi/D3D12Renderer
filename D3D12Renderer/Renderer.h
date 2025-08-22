#pragma once

#include <string>
#include <Windows.h>

class Renderer {
public:
    Renderer(UINT width, UINT height, std::wstring name);
    ~Renderer();

    UINT GetWidth() const { return m_width; }
    UINT GetHeight() const { return m_height; }
    const WCHAR* GetTitle() const { return m_title.c_str(); }

private:
    UINT m_width;
    UINT m_height;
    float m_aspectRatio;

    std::wstring m_title;
};