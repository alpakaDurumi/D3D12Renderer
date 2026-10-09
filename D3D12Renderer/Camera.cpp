#include "pch.h"

#include "Camera.h"

#include <DirectXCollision.h>

using namespace DirectX;

Camera::Camera(XMFLOAT3 initialPosition)
    : m_position(initialPosition)
{
    m_yaw = 0.0f;
    m_pitch = 0.0f;

    m_aspectRatio = 16.0f / 9.0f;
    SetHorizontalFov(XMConvertToRadians(90.0f));
    m_nearPlane = 0.1f;
    m_farPlane = 1000.0f;
}

XMVECTOR Camera::GetPosition() const
{
    return XMVectorSetW(XMLoadFloat3(&m_position), 1.0f);
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
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR rot = XMLoadFloat4(&m_rotation);

    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rot);
    XMVECTOR up = XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rot);

    return XMMatrixLookToLH(pos, forward, up);
}

// Return reverse-z projection matrix.
XMMATRIX Camera::GetProjectionMatrix(bool usePerspectiveProjection) const
{
    return usePerspectiveProjection ? XMMatrixPerspectiveFovLH(m_verticalFov, m_aspectRatio, m_farPlane, m_nearPlane) : XMMatrixOrthographicLH(2 * m_aspectRatio, 2.0f, m_farPlane, m_nearPlane);
}

// Create bounding frustum of view frustum and transform to world space.
// Uses a standard projection matrix, not a reverse-z projection matrix.
BoundingFrustum Camera::GetWorldFrustum() const
{
    BoundingFrustum boundingFrustum;
    XMMATRIX projection = XMMatrixPerspectiveFovLH(m_verticalFov, m_aspectRatio, m_nearPlane, m_farPlane);
    BoundingFrustum::CreateFromMatrix(boundingFrustum, projection);

    XMMATRIX inverseView = XMMatrixInverse(nullptr, GetViewMatrix());
    boundingFrustum.Transform(boundingFrustum, inverseView);

    return boundingFrustum;
}

void XM_CALLCONV Camera::SetPosition(FXMVECTOR pos)
{
    XMStoreFloat3(&m_position, pos);
}

void Camera::SetAspectRatio(float aspectRatio)
{
    m_aspectRatio = aspectRatio;
    m_verticalFov = CalcVerticalFov(m_horizontalFov);
}

void Camera::SetHorizontalFov(float horizontalFov)
{
    m_horizontalFov = horizontalFov;
    m_verticalFov = CalcVerticalFov(horizontalFov);
}

void Camera::MoveForward(float speedScale)
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR rot = XMLoadFloat4(&m_rotation);

    XMVECTOR forward = XMVector3Rotate(XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rot);

    pos += forward * speedScale;

    XMStoreFloat3(&m_position, pos);
}

void Camera::MoveRight(float speedScale)
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR rot = XMLoadFloat4(&m_rotation);

    XMVECTOR right = XMVector3Rotate(XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rot);

    pos += right * speedScale;

    XMStoreFloat3(&m_position, pos);
}

void Camera::MoveUp(float speedScale)
{
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    pos += up * speedScale;

    XMStoreFloat3(&m_position, pos);
}

void Camera::Rotate(XMINT2 mouseMove)
{
    constexpr float angularSensitivity = 0.0035f;

    m_yaw += mouseMove.x * angularSensitivity;
    m_pitch += mouseMove.y * angularSensitivity;
    m_pitch = std::clamp(m_pitch, -XM_PIDIV2 + 0.01f, XM_PIDIV2 - 0.01f);

    XMVECTOR q = XMQuaternionRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);
    XMStoreFloat4(&m_rotation, q);
}

void XM_CALLCONV Camera::Orbit(FXMVECTOR pivot, float distance, XMINT2 mouseMove)
{
    Rotate(mouseMove);
    XMVECTOR newPos = pivot - GetForward() * distance;
    XMStoreFloat3(&m_position, newPos);
}

void Camera::Pan(XMINT2 mouseMove)
{
    constexpr float panSensitivity = 0.01f;

    MoveRight(mouseMove.x * panSensitivity);

    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR rot = XMLoadFloat4(&m_rotation);
    XMVECTOR localUp = XMVector3Rotate(XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rot);

    pos += XMVectorScale(localUp, -mouseMove.y * panSensitivity);

    XMStoreFloat3(&m_position, pos);
}

Ray Camera::GetRay(XMFLOAT2 coord, XMFLOAT2 resolution) const
{
    // Screen space -> NDC -> world space
    float ndcX = 2.0f * coord.x / resolution.x - 1.0f;
    float ndcY = 1.0f - 2.0f * coord.y / resolution.y;

    XMFLOAT4X4 proj;
    XMStoreFloat4x4(&proj, GetProjectionMatrix());
    XMVECTOR dirView = XMVectorSet(ndcX / proj._11, ndcY / proj._22, 1.0f, 0.0f);
    XMVECTOR dirWorld = XMVector3Normalize(XMVector3Rotate(dirView, XMLoadFloat4(&m_rotation)));

    return {XMLoadFloat3(&m_position), dirWorld};
}

float Camera::CalcVerticalFov(float horizontalFov)
{
    assert(m_aspectRatio > 0.0f);

    return 2.0f * std::atan2(std::tan(horizontalFov * 0.5f), m_aspectRatio);
}
