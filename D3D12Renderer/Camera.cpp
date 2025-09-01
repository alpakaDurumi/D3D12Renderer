#include "Camera.h"

Camera::Camera(XMFLOAT3 initialPosition) {
    m_position = initialPosition;
    m_orientation = { 0.0f, 0.0f, 1.0f };
    m_up = { 0.0f, 1.0f, 0.0f };
    m_yaw = 0.0f;
    m_pitch = 0.0f;
}
