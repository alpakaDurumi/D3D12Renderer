#pragma once

#include <Windows.h>

#include <DirectXMath.h>

#include <cstring>

#include "Aliases.h"

class InputManager
{
public:
    void SetKeyDown(VKCode key)
    {
        if (key < 256)
        {
            m_keyDown[key] = true;
            m_keyPressed[key] = true;
        }
    }

    void SetKeyUp(VKCode key)
    {
        if (key < 256) m_keyDown[key] = false;
    }

    bool IsKeyDown(VKCode key) const
    {
        return m_keyDown[key];
    }

    bool IsKeyPressed(VKCode key) const
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
        if (!(m_lastMousePos.x == -1 && m_lastMousePos.y == -1))
        {
            m_mouseMove.x += posX - m_lastMousePos.x;
            m_mouseMove.y += posY - m_lastMousePos.y;
        }

        m_lastMousePos.x = posX;
        m_lastMousePos.y = posY;
    }

    XMINT2 GetAndResetMouseMove()
    {
        XMINT2 delta = m_mouseMove;
        m_mouseMove = { 0, 0 };
        return delta;
    }

    void AccumulateMouseWheelStep(float step)
    {
        m_accumulatedMouseWheelStep += step;
    }

    float GetAndResetMouseWheelStep()
    {
        float step = m_accumulatedMouseWheelStep;
        m_accumulatedMouseWheelStep = 0.0f;
        return step;
    }

    void ResetPressedFlags()
    {
        memset(m_keyPressed, false, sizeof(m_keyPressed));
        memset(m_mouseButtonPressed, false, sizeof(m_mouseButtonPressed));
    }

    void Reset()
    {
        memset(m_keyDown, false, sizeof(m_keyDown));
        memset(m_mouseButtonDown, false, sizeof(m_mouseButtonDown));
        ResetPressedFlags();
        m_lastMousePos = { -1, -1 };
        m_mouseMove = { 0, 0 };
        m_accumulatedMouseWheelStep = 0.0f;
    }

private:
    // Keyboard
    bool m_keyDown[256] = { false };
    bool m_keyPressed[256] = { false };

    // Mouse
    // 0 = LMB, 1 = RMB, 2 = MMB
    bool m_mouseButtonDown[3] = { false };
    bool m_mouseButtonPressed[3] = { false };
    DirectX::XMINT2 m_lastMousePos = { -1, -1 };
    DirectX::XMINT2 m_mouseMove = { 0, 0 };
    float m_accumulatedMouseWheelStep = 0.0f;
};