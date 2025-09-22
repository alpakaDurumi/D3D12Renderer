#pragma once

// 여러 곳으로 동작을 전파할 때, 콜백/함수 포인터/이벤트 시스템 등 여러가지 방법 가능
class InputManager
{
public:
    void SetKeyDown(WPARAM key)
    {
        if (key < 256)
        {
            m_keyPressed[key] = true;
            m_keyDown[key] = true;
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

    void CalcMouseMove(int posX, int posY)
    {
        if (!(m_lastPosX == -1 && m_lastPosY == -1))
            m_mouseMove = { posX - m_lastPosX, posY - m_lastPosY };

        m_lastPosX = posX;
        m_lastPosY = posY;
    }

    XMINT2 GetAndResetMouseMove()
    {
        XMINT2 delta = m_mouseMove;
        m_mouseMove = { 0, 0 };
        return delta;
    }

    void OnFrameEnd()
    {
        memset(m_keyPressed, false, sizeof(m_keyPressed));
    }

private:
    bool m_keyDown[256] = { false };
    bool m_keyPressed[256] = { false };
    int m_lastPosX = -1;
    int m_lastPosY = -1;
    XMINT2 m_mouseMove = { 0, 0 };
};