#pragma once

#include <DirectXMath.h>

namespace DirectX
{
struct BoundingFrustum;
}

struct Ray
{
    DirectX::XMVECTOR origin, dir;
};

class Camera
{
public:
    Camera(DirectX::XMFLOAT3 initialPosition = {0.0f, 0.0f, 0.0f});

    DirectX::XMVECTOR GetPosition() const;
    DirectX::XMVECTOR GetForward() const;
    float GetNearPlane() const;
    float GetFarPlane() const;
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix(bool usePerspectiveProjection = true) const;
    DirectX::BoundingFrustum GetWorldFrustum() const;

    void SetPosition(const DirectX::XMVECTOR& pos);
    void SetAspectRatio(float aspectRatio);
    void SetHorizontalFov(float horizontalFov);

    void MoveForward(float speedScale);
    void MoveRight(float speedScale);
    void MoveUp(float speedScale);
    void Rotate(DirectX::XMINT2 mouseMove);
    void Orbit(DirectX::XMVECTOR pivot, float distance, DirectX::XMINT2 mouseMove);
    void Pan(DirectX::XMINT2 mouseMove);
    Ray GetRay(DirectX::XMFLOAT2 coord, DirectX::XMFLOAT2 resolution) const;

private:
    float CalcVerticalFov(float horizontalFov);

    DirectX::XMFLOAT3 m_position;

    float m_yaw;
    float m_pitch;
    DirectX::XMFLOAT4 m_rotation = {0.0f, 0.0f, 0.0f, 1.0f}; // quaternion

    float m_verticalFov;
    float m_horizontalFov;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;
};
