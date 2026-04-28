#pragma once

#include <DirectXMath.h>

using namespace DirectX;

// Scene Editor Camera
// Position uses fixed timestep interpolation for smoothness.
// Rotation uses immediate input response for low latency.
class Camera
{
public:
    Camera(XMFLOAT3 initialPosition = { 0.0f, 0.0f, 0.0f });

    void UpdateRenderState(float alpha);

    XMVECTOR GetCurrentPosition() const;
    XMVECTOR GetRenderPosition() const;
    XMVECTOR GetForward() const;
    float GetNearPlane() const;
    float GetFarPlane() const;
    XMMATRIX GetViewMatrix() const;
    XMMATRIX GetProjectionMatrix(bool usePerspectiveProjection = true) const;

    void SetAspectRatio(float aspectRatio);
    void SetHorizontalFOV(float horizontalFOV);

    void SnapshotState();
    void MoveForward(float speedScale);
    void MoveRight(float speedScale);
    void MoveUp(float speedScale);
    void Rotate(XMINT2 mouseMove);
    void Orbit(XMVECTOR pivot, float distance, XMINT2 mouseMove);

private:
    float CalcVerticalFOV(float horizontalFOV);

    XMFLOAT3 m_prevPosition;
    XMFLOAT3 m_currPosition;
    XMFLOAT3 m_renderPosition;

    float m_yaw;
    float m_pitch;
    XMFLOAT4 m_rotation = { 0.0f, 0.0f, 0.0f, 1.0f };    // quaternion

    float m_verticalFOV;
    float m_horizontalFOV;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;
};