#pragma once

#include <Windows.h>

#include <DirectXMath.h>

#include <cstring>

class InputManager
{
public:
    void SetKeyDown(WPARAM key)
    {
        if (key < 256)
        {
            m_keyDown[key] = true;
            m_keyPressed[key] = true;
        }
    }

    void SetKeyUp(WPARAM key)
    {
        if (key < 256) m_keyDown[key] = false;
    }

    bool IsKeyDown(WPARAM key) const
    {
        return m_keyDown[key];
    }

    bool IsKeyPressed(WPARAM key) const
    {
        return m_keyPressed[key];
    }

    void SetMouseButtonDown(UINT button)
    {
        m_mouseButtonDown[button] = true;
        m_mouseButtonPressed[button] = true;
    }

    void SetMouseButtonUp(UINT button)
    {
        m_mouseButtonDown[button] = false;
    }

    bool IsMouseButtonDown(UINT button) const
    {
        return m_mouseButtonDown[button];
    }

    bool IsMouseButtonPressed(UINT button) const
    {
        return m_mouseButtonPressed[button];
    }

    // Accumulate mouse moves.
    void CalcMouseMove(int posX, int posY)
    {
        if (!(m_lastPosX == -1 && m_lastPosY == -1))
        {
            m_mouseMove.x += posX - m_lastPosX;
            m_mouseMove.y += posY - m_lastPosY;
        }

        m_lastPosX = posX;
        m_lastPosY = posY;
    }

    XMINT2 GetAndResetMouseMove()
    {
        XMINT2 delta = m_mouseMove;
        m_mouseMove = { 0, 0 };
        return delta;
    }

    void ResetPressedFlags()
    {
        memset(m_keyPressed, false, sizeof(m_keyPressed));
        memset(m_mouseButtonPressed, false, sizeof(m_mouseButtonPressed));
    }

private:
    bool m_keyDown[256] = { false };
    bool m_keyPressed[256] = { false };
    int m_lastPosX = -1;
    int m_lastPosY = -1;

    // 0 = LMB, 1 = RMB, 2 = MMB
    bool m_mouseButtonDown[3] = { false };
    bool m_mouseButtonPressed[3] = { false };
    XMINT2 m_mouseMove = { 0, 0 };
};