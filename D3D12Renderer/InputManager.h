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
    void CalcMouseMove(int dx, int dy, int cx, int cy)
    {
        m_mouseMove.x += dx;
        m_mouseMove.y += dy;

        m_mousePos = { cx, cy };
    }

    XMINT2 GetAndResetMouseMove()
    {
        XMINT2 delta = m_mouseMove;
        m_mouseMove = { 0, 0 };
        return delta;
    }

    void AccumulateMouseWheelStep(float stepDelta)
    {
        m_accumulatedMouseWheelStep += stepDelta;
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
        m_mouseMove = { 0, 0 };
        m_mousePos = { 0, 0 };
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
    DirectX::XMINT2 m_mouseMove = { 0, 0 };
    DirectX::XMINT2 m_mousePos = { 0, 0 };
    float m_accumulatedMouseWheelStep = 0.0f;
};