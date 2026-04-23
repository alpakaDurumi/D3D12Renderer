#pragma once

#include <Windows.h>

#include <DirectXMath.h>

#include "InstanceData.h"

using namespace DirectX;

class Transform
{
public:
    Transform()
    {
        m_prevS = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_currS = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_prevR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_currR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_prevT = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_currT = XMFLOAT3(0.0f, 0.0f, 0.0f);

        XMStoreFloat4x4(&m_localRenderTransform, XMMatrixIdentity());
    }

    Transform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
    {
        m_prevS = m_currS = s;

        XMVECTOR r = XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
        XMStoreFloat4(&m_prevR, r);
        m_currR = m_prevR;

        m_prevT = m_currT = t;

        XMStoreFloat4x4(&m_localRenderTransform, XMMatrixIdentity());
    }

    void SnapshotState()
    {
        m_prevS = m_currS;
        m_prevR = m_currR;
        m_prevT = m_currT;
    }

    // Accumulate each component
    void Apply(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
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
    void UpdateLocalRenderState(float alpha)
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

    const XMFLOAT4X4& GetLocalRenderTransform() const
    {
        return m_localRenderTransform;
    }

    void SetWorldRenderTransform(const XMMATRIX& transform)
    {
        XMStoreFloat4x4(&m_worldRenderTransform, transform);
    }

    XMFLOAT4X4 GetWorldRenderTransform() const
    {
        return m_worldRenderTransform;
    }

    XMFLOAT3 GetScale() const
    {
        return m_currS;
    }

    void SetScale(const XMFLOAT3& s)
    {
        m_currS = s;
        m_prevS = s;
    }

    // If selected entitiy changed, calculate euler angles from quaternion
    XMFLOAT3 GetEulerCache(bool selectionChanged)
    {
        if (selectionChanged)
            m_eulerCache = QuaternionToEuler(m_currR);

        return m_eulerCache;
    }

    // Convert euler angles to quaternion
    void SetRotation(const XMFLOAT3& eulerRDeg)
    {
        m_eulerCache = eulerRDeg;

        float pitch = XMConvertToRadians(eulerRDeg.x);
        float yaw = XMConvertToRadians(eulerRDeg.y);
        float roll = XMConvertToRadians(eulerRDeg.z);

        XMVECTOR r = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        XMStoreFloat4(&m_currR, r);
        XMStoreFloat4(&m_prevR, r);
    }

    XMFLOAT3 GetTranslation() const
    {
        return m_currT;
    }

    void SetTranslation(const XMFLOAT3& t)
    {
        m_currT = t;
        m_prevT = t;
    }

    // Assume that order of rotation is roll -> pitch -> yaw
    XMFLOAT3 QuaternionToEuler(const XMFLOAT4& q) const
    {
        // pitch φ (X): M_32 = -sinφ
        float sinPhi = 2.0f * (q.w * q.x - q.y * q.z);  // 2(wx - yz)
        float pitch = XMConvertToDegrees(
            fabsf(sinPhi) >= 1.0f ? copysignf(XM_PIDIV2, sinPhi) : asinf(sinPhi));

        // yaw θ (Y): atan2(M_31, M_33)
        float yaw = XMConvertToDegrees(
            atan2f(2.0f * (q.x * q.z + q.w * q.y),
                1.0f - 2.0f * (q.x * q.x + q.y * q.y)));

        // roll ψ (Z): atan2(M_12, M_22)
        float roll = XMConvertToDegrees(
            atan2f(2.0f * (q.x * q.y + q.w * q.z),
                1.0f - 2.0f * (q.x * q.x + q.z * q.z)));

        return { pitch, yaw, roll };
    }

private:
    XMFLOAT3 m_prevS, m_currS;
    XMFLOAT4 m_prevR, m_currR;
    XMFLOAT3 m_prevT, m_currT;

    XMFLOAT4X4 m_localRenderTransform;
    XMFLOAT4X4 m_worldRenderTransform;

    XMFLOAT3 m_eulerCache;
};