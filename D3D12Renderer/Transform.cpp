#include "pch.h"

#include "Transform.h"

#include <cmath>

#include <DirectXMath.h>

using namespace DirectX;

// Assume that order of rotation is roll -> pitch -> yaw
static XMFLOAT3 QuaternionToEuler(const XMFLOAT4& q)
{
    // pitch φ (X): M_32 = -sinφ
    float sinPhi = 2.0f * (q.w * q.x - q.y * q.z); // 2(wx - yz)
    float pitch = XMConvertToDegrees(
        std::fabsf(sinPhi) >= 1.0f ? std::copysignf(XM_PIDIV2, sinPhi) : std::asinf(sinPhi));

    // yaw θ (Y): atan2(M_31, M_33)
    float yaw = XMConvertToDegrees(
        std::atan2f(2.0f * (q.x * q.z + q.w * q.y),
                    1.0f - 2.0f * (q.x * q.x + q.y * q.y)));

    // roll ψ (Z): atan2(M_12, M_22)
    float roll = XMConvertToDegrees(
        std::atan2f(2.0f * (q.x * q.y + q.w * q.z),
                    1.0f - 2.0f * (q.x * q.x + q.z * q.z)));

    return {pitch, yaw, roll};
}

Transform::Transform()
{
    m_prevS = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_currS = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_prevR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    m_currR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    m_prevT = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_currT = XMFLOAT3(0.0f, 0.0f, 0.0f);

    XMStoreFloat4x4(&m_localRenderTransform, XMMatrixIdentity());
}

Transform::Transform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
{
    m_prevS = m_currS = s;

    XMVECTOR r = XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
    XMStoreFloat4(&m_prevR, r);
    m_currR = m_prevR;

    m_prevT = m_currT = t;

    XMStoreFloat4x4(&m_localRenderTransform, XMMatrixIdentity());
}

// Accumulate each component
void Transform::Apply(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
{
    // S
    m_currS.x *= s.x;
    m_currS.y *= s.y;
    m_currS.z *= s.z;

    // R
    XMVECTOR currR = XMLoadFloat4(&m_currR);
    XMVECTOR deltaR = XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
    currR = XMQuaternionNormalize(XMQuaternionMultiply(currR, deltaR));
    XMStoreFloat4(&m_currR, currR);

    // T
    m_currT.x += t.x;
    m_currT.y += t.y;
    m_currT.z += t.z;
}

// Calculate local transform
void Transform::UpdateLocalRenderState(float alpha)
{
    XMVECTOR prevS = XMVectorSetW(XMLoadFloat3(&m_prevS), 0.0f);
    XMVECTOR currS = XMVectorSetW(XMLoadFloat3(&m_currS), 0.0f);
    XMVECTOR prevR = XMLoadFloat4(&m_prevR);
    XMVECTOR currR = XMLoadFloat4(&m_currR);
    XMVECTOR prevT = XMVectorSetW(XMLoadFloat3(&m_prevT), 0.0f);
    XMVECTOR currT = XMVectorSetW(XMLoadFloat3(&m_currT), 0.0f);

    XMVECTOR renderS = XMVectorLerp(prevS, currS, alpha);
    XMVECTOR renderR = XMQuaternionSlerp(prevR, currR, alpha);
    XMVECTOR renderT = XMVectorLerp(prevT, currT, alpha);

    XMStoreFloat4x4(&m_localRenderTransform, XMMatrixAffineTransformation(renderS, XMVectorZero(), renderR, renderT));
}

// If selected entity changed, calculate euler angles from quaternion
XMFLOAT3 Transform::GetEulerCache(bool selectionChanged)
{
    if (selectionChanged)
        m_eulerCache = QuaternionToEuler(m_currR);

    return m_eulerCache;
}

// Convert euler angles to quaternion
void Transform::SetRotation(const XMFLOAT3& eulerRDeg)
{
    m_eulerCache = eulerRDeg;

    float pitch = XMConvertToRadians(eulerRDeg.x);
    float yaw = XMConvertToRadians(eulerRDeg.y);
    float roll = XMConvertToRadians(eulerRDeg.z);

    XMVECTOR r = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
    XMStoreFloat4(&m_currR, r);
    XMStoreFloat4(&m_prevR, r);
}

void Transform::SetForward(const XMFLOAT3& forward)
{
    XMVECTOR from = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    XMVECTOR to = XMVector3Normalize(XMLoadFloat3(&forward));

    XMVECTOR cross = XMVector3Cross(from, to);
    float dot = XMVectorGetX(XMVector3Dot(from, to));

    XMVECTOR q;

    if (dot < -0.999999f)
        q = XMQuaternionRotationRollPitchYaw(0.0f, XM_PI, 0.0f);
    else
        q = XMQuaternionNormalize(XMVectorSetW(cross, 1.0f + dot));

    XMStoreFloat4(&m_prevR, q);
    XMStoreFloat4(&m_currR, q);
}
