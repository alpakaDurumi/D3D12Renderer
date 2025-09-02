#pragma once

#include <DirectXMath.h>

using namespace DirectX;

class Camera {
public:
    Camera(float aspectRatio, XMFLOAT3 initialPosition = { 0.0f, 0.0f, 0.0f });
    XMMATRIX GetViewMatrix();
    XMMATRIX GetProjectionMatrix(bool usePerspectiveProjection);
    void SetAspectRatio(float aspectRatio);
    void MoveForward(float speedScale);
    void MoveRight(float speedScale);
    void MoveUp(float speedScale);

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