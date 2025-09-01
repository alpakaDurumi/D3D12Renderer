#pragma once

#include <DirectXMath.h>

using namespace DirectX;

class Camera {
public:
    Camera(XMFLOAT3 initialPosition = { 0.0f, 0.0f, 0.0f });

private:
    XMFLOAT3 m_position;
    XMFLOAT3 m_orientation;
    XMFLOAT3 m_up;

    float m_yaw;
    float m_pitch;
};