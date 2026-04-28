#include "pch.h"
#include "Camera.h"

Camera::Camera(XMFLOAT3 initialPosition)
{
    m_prevPosition = initialPosition;
    m_currPosition = initialPosition;

    m_yaw = 0.0f;
    m_pitch = 0.0f;

    m_aspectRatio = 16.0f / 9.0f;
    SetHorizontalFOV(XMConvertToRadians(90.0f));
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

XMVECTOR Camera::GetCurrentPosition() const
{
    return XMVectorSetW(XMLoadFloat3(&m_currPosition), 1.0f);
}

XMVECTOR Camera::GetRenderPosition() const
{
    return XMVectorSetW(XMLoadFloat3(&m_renderPosition), 1.0f);
}

XMVECTOR Camera::GetForward() const
{
    XMVECTOR rot = XMLoadFloat4(&m_rotation);
    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rot);
    return forward;
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
    XMVECTOR pos = XMLoadFloat3(&m_renderPosition);
    XMVECTOR rot = XMLoadFloat4(&m_rotation);

    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rot);
    XMVECTOR up = XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rot);

    return XMMatrixLookToLH(pos, forward, up);
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
    XMVECTOR pos = XMLoadFloat3(&m_currPosition);
    XMVECTOR rot = XMLoadFloat4(&m_rotation);

    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rot);

    pos += forward * speedScale;

    XMStoreFloat3(&m_currPosition, pos);
}

void Camera::MoveRight(float speedScale)
{
    XMVECTOR pos = XMLoadFloat3(&m_currPosition);
    XMVECTOR rot = XMLoadFloat4(&m_rotation);

    XMVECTOR right = XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rot);

    pos += right * speedScale;

    XMStoreFloat3(&m_currPosition, pos);
}

void Camera::MoveUp(float speedScale)
{
    XMVECTOR pos = XMLoadFloat3(&m_currPosition);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    pos += up * speedScale;

    XMStoreFloat3(&m_currPosition, pos);
}

void Camera::Rotate(XMINT2 mouseMove)
{
    const float angularSensitivity = 0.0035f;

    m_yaw += mouseMove.x * angularSensitivity;
    m_pitch += mouseMove.y * angularSensitivity;
    m_pitch = std::clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

    XMVECTOR q = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);
    XMStoreFloat4(&m_rotation, q);
}

void Camera::Orbit(XMVECTOR pivot, float distance, XMINT2 mouseMove)
{
    Rotate(mouseMove);
    XMVECTOR newPos = pivot - GetForward() * distance;
    XMStoreFloat3(&m_currPosition, newPos);
}

float Camera::CalcVerticalFOV(float horizontalFOV)
{
    assert(m_aspectRatio > 0.0f);

    return 2.0f * std::atan2(std::tan(horizontalFOV * 0.5f), m_aspectRatio);
}