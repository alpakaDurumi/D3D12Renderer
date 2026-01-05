#pragma once

#include <DirectXMath.h>

using namespace DirectX;

class Camera {
public:
    Camera(XMFLOAT3 initialPosition = { 0.0f, 0.0f, 0.0f });
    XMFLOAT3 GetPosition();
    XMMATRIX GetViewMatrix();
    XMMATRIX GetProjectionMatrix(bool usePerspectiveProjection = true);
    void SetAspectRatio(float aspectRatio);
    void MoveForward(float speedScale);
    void MoveRight(float speedScale);
    void MoveUp(float speedScale);
    void Rotate(XMINT2 mouseMove);

private:
    XMFLOAT3 m_position;
    XMFLOAT3 m_orientation;
    XMFLOAT3 m_up;

    float m_yaw;
    float m_pitch;

    float m_fov;
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;
};