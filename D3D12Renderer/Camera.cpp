#include "pch.h"
#include "Camera.h"

Camera::Camera(XMFLOAT3 initialPosition)
{
    m_prevPosition = initialPosition;
    m_currPosition = initialPosition;
    m_orientation = { 0.0f, 0.0f, 1.0f };
    m_up = { 0.0f, 1.0f, 0.0f };

    m_yaw = 0.0f;
    m_pitch = 0.0f;

    m_aspectRatio = 16.0f / 9.0f;
    SetHorizontalFOV(90.0f);
    m_nearPlane = 0.1f;
    m_farPlane = 1000.0f;
}

void Camera::UpdateRenderState(float alpha)
{
    XMVECTOR prev = XMLoadFloat3(&m_prevPosition);
    XMVECTOR curr = XMLoadFloat3(&m_currPosition);
    XMVECTOR interpolated = XMVectorLerp(prev, curr, alpha);
    XMStoreFloat3(&m_renderPosition, interpolated);
}

XMVECTOR Camera::GetPosition() const
{
    return XMVectorSetW(XMLoadFloat3(&m_renderPosition), 1.0f);
}

float Camera::GetNearPlane() const
{
    return m_nearPlane;
}

float Camera::GetFarPlane() const
{
    return m_farPlane;
}

XMMATRIX Camera::GetViewMatrix() const
{
    return XMMatrixLookToLH(XMLoadFloat3(&m_renderPosition), XMLoadFloat3(&m_orientation), XMLoadFloat3(&m_up));
}

XMMATRIX Camera::GetProjectionMatrix(bool usePerspectiveProjection) const
{
    return usePerspectiveProjection ?
        XMMatrixPerspectiveFovLH(m_verticalFOV, m_aspectRatio, m_farPlane, m_nearPlane) :
        XMMatrixOrthographicLH(2 * m_aspectRatio, 2.0f, m_farPlane, m_nearPlane);
}

void Camera::SetAspectRatio(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    m_verticalFOV = CalcVerticalFOV(m_horizontalFOV);
}

void Camera::SetHorizontalFOV(float horizontalFOV)
{
    m_horizontalFOV = horizontalFOV;
    m_verticalFOV = CalcVerticalFOV(horizontalFOV);
}

void Camera::SnapshotState()
{
    m_prevPosition = m_currPosition;
}

void Camera::MoveForward(float speedScale)
{
    XMVECTOR position = XMLoadFloat3(&m_currPosition);
    XMVECTOR orientation = XMLoadFloat3(&m_orientation);

    XMVECTOR nextPosition = position + orientation * speedScale;

    XMStoreFloat3(&m_currPosition, nextPosition);
}

void Camera::MoveRight(float speedScale)
{
    XMVECTOR position = XMLoadFloat3(&m_currPosition);
    XMVECTOR orientation = XMLoadFloat3(&m_orientation);
    XMVECTOR up = XMLoadFloat3(&m_up);

    XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, orientation));

    XMVECTOR nextPosition = position + right * speedScale;

    XMStoreFloat3(&m_currPosition, nextPosition);
}

void Camera::MoveUp(float speedScale)
{
    XMVECTOR position = XMLoadFloat3(&m_currPosition);
    XMVECTOR up = XMLoadFloat3(&m_up);

    XMVECTOR nextPosition = position + up * speedScale;

    XMStoreFloat3(&m_currPosition, nextPosition);
}

void Camera::Rotate(XMINT2 mouseMove)
{
    const float sensitivity = 0.005f;
    
    m_yaw += mouseMove.x * sensitivity * XM_PIDIV2;
    m_pitch += mouseMove.y * sensitivity * XM_PIDIV2;

    m_pitch = std::clamp(m_pitch, -XM_PIDIV2, XM_PIDIV2);

    XMVECTOR lookDirection = XMVector3Transform(g_XMIdentityR2, XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.0f));
    XMStoreFloat3(&m_orientation, lookDirection);
}

float Camera::CalcVerticalFOV(float horizontalFOV)
{
    assert(m_aspectRatio > 0.0f);

    return 2.0f * std::atan2(std::tan(XMConvertToRadians(horizontalFOV * 0.5f)), m_aspectRatio);
}