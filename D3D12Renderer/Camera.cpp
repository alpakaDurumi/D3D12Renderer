#include "Camera.h"

Camera::Camera(float aspectRatio, XMFLOAT3 initialPosition) {
    m_position = initialPosition;
    m_orientation = { 0.0f, 0.0f, 1.0f };
    m_up = { 0.0f, 1.0f, 0.0f };

    m_yaw = 0.0f;
    m_pitch = 0.0f;

    m_fov = 90.0f * 0.5f;
    m_aspectRatio = aspectRatio;
    m_nearPlane = 0.01f;
    m_farPlane = 100.0f;
}

XMMATRIX Camera::GetViewMatrix() {
    return XMMatrixLookToLH(XMLoadFloat3(&m_position), XMLoadFloat3(&m_orientation), XMLoadFloat3(&m_up));
}

XMMATRIX Camera::GetProjectionMatrix(bool usePerspectiveProjection) {
    return usePerspectiveProjection ?
        XMMatrixPerspectiveFovLH(XMConvertToRadians(m_fov), m_aspectRatio, m_nearPlane, m_farPlane) :
        XMMatrixOrthographicLH(2 * m_aspectRatio, 2.0f, m_nearPlane, m_farPlane);
}

void Camera::SetAspectRatio(float aspectRatio) {
    m_aspectRatio = aspectRatio;
}