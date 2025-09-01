#pragma once

// 여러 곳으로 동작을 전파할 때, 콜백/함수 포인터/이벤트 시스템 등 여러가지 방법 가능
class InputManager
{
public:
    void SetKeyDown(WPARAM key)
    {
        if (key < 256) m_keyStates[key] = true;
    }

    void SetKeyUp(WPARAM key)
    {
        if (key < 256) m_keyStates[key] = false;
    }

    bool isKeyDown(WPARAM key) const
    {
        return m_keyStates[key];
    }

private:
    bool m_keyStates[256] = { false };
};