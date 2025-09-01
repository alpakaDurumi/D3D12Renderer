#pragma once

// ���� ������ ������ ������ ��, �ݹ�/�Լ� ������/�̺�Ʈ �ý��� �� �������� ��� ����
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

private:
    bool m_keyStates[256] = { false };
};