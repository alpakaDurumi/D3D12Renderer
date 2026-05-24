#pragma once

#include <DirectXMath.h>

// Scene Editor Camera
// Position uses fixed timestep interpolation for smoothness.
// Rotation uses immediate input response for low latency.
class Camera
{
public:
    Camera(DirectX::XMFLOAT3 initialPosition = {0.0f, 0.0f, 0.0f});

    void UpdateRenderState(float alpha);

    DirectX::XMVECTOR GetCurrentPosition() const;
    DirectX::XMVECTOR GetRenderPosition() const;
    DirectX::XMVECTOR GetForward() const;
    float GetNearPlane() const;
    float GetFarPlane() const;
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMMATRIX GetProjectionMatrix(bool usePerspectiveProjection = true) const;

    void SetCurrentPosition(const DirectX::XMVECTOR& pos);
    void SetAspectRatio(float aspectRatio);
    void SetHorizontalFov(float horizontalFov);

    void SnapshotState();
    void MoveForward(float speedScale);
    void MoveRight(float speedScale);
    void MoveUp(float speedScale);
    void Rotate(DirectX::XMINT2 mouseMove);
    void Orbit(DirectX::XMVECTOR pivot, float distance, DirectX::XMINT2 mouseMove);
    void Pan(DirectX::XMINT2 mouseMove);

private:
    float CalcVerticalFov(float horizontalFov);

    DirectX::XMFLOAT3 m_prevPosition;
    DirectX::XMFLOAT3 m_currPosition;
    DirectX::XMFLOAT3 m_renderPosition;

    float m_yaw;
    float m_pitch;
    DirectX::XMFLOAT4 m_rotation = {0.0f, 0.0f, 0.0f, 1.0f}; // quaternion

    float m_verticalFov;
    float m_horizontalFov;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;
};
