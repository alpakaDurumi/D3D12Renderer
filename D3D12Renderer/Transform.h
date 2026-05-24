#pragma once

#include <cmath>

#include <DirectXMath.h>

class Transform
{
public:
    Transform()
    {
        m_prevS = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_currS = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_prevR = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_currR = DirectX::XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_prevT = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_currT = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

        DirectX::XMStoreFloat4x4(&m_localRenderTransform, DirectX::XMMatrixIdentity());
    }

    Transform(const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& eulerRad, const DirectX::XMFLOAT3& t)
    {
        m_prevS = m_currS = s;

        DirectX::XMVECTOR r = DirectX::XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
        DirectX::XMStoreFloat4(&m_prevR, r);
        m_currR = m_prevR;

        m_prevT = m_currT = t;

        DirectX::XMStoreFloat4x4(&m_localRenderTransform, DirectX::XMMatrixIdentity());
    }

    void SnapshotState()
    {
        m_prevS = m_currS;
        m_prevR = m_currR;
        m_prevT = m_currT;
    }

    // Accumulate each component
    void Apply(const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& eulerRad, const DirectX::XMFLOAT3& t)
    {
        // S
        m_currS.x *= s.x;
        m_currS.y *= s.y;
        m_currS.z *= s.z;

        // R
        DirectX::XMVECTOR currR = DirectX::XMLoadFloat4(&m_currR);
        DirectX::XMVECTOR deltaR = DirectX::XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
        currR = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(currR, deltaR));
        DirectX::XMStoreFloat4(&m_currR, currR);

        // T
        m_currT.x += t.x;
        m_currT.y += t.y;
        m_currT.z += t.z;
    }

    // Calculate local transform
    void UpdateLocalRenderState(float alpha)
    {
        DirectX::XMVECTOR prevS = DirectX::XMVectorSetW(DirectX::XMLoadFloat3(&m_prevS), 0.0f);
        DirectX::XMVECTOR currS = DirectX::XMVectorSetW(DirectX::XMLoadFloat3(&m_currS), 0.0f);
        DirectX::XMVECTOR prevR = DirectX::XMLoadFloat4(&m_prevR);
        DirectX::XMVECTOR currR = DirectX::XMLoadFloat4(&m_currR);
        DirectX::XMVECTOR prevT = DirectX::XMVectorSetW(DirectX::XMLoadFloat3(&m_prevT), 0.0f);
        DirectX::XMVECTOR currT = DirectX::XMVectorSetW(DirectX::XMLoadFloat3(&m_currT), 0.0f);

        DirectX::XMVECTOR renderS = DirectX::XMVectorLerp(prevS, currS, alpha);
        DirectX::XMVECTOR renderR = DirectX::XMQuaternionSlerp(prevR, currR, alpha);
        DirectX::XMVECTOR renderT = DirectX::XMVectorLerp(prevT, currT, alpha);

        DirectX::XMStoreFloat4x4(&m_localRenderTransform, DirectX::XMMatrixAffineTransformation(renderS, DirectX::XMVectorZero(), renderR, renderT));
    }

    const DirectX::XMFLOAT4X4& GetLocalRenderTransform() const
    {
        return m_localRenderTransform;
    }

    void SetWorldRenderTransform(const DirectX::XMMATRIX& transform)
    {
        DirectX::XMStoreFloat4x4(&m_worldRenderTransform, transform);
    }

    DirectX::XMFLOAT4X4 GetWorldRenderTransform() const
    {
        return m_worldRenderTransform;
    }

    DirectX::XMFLOAT3 GetScale() const
    {
        return m_currS;
    }

    void SetScale(const DirectX::XMFLOAT3& s)
    {
        m_currS = s;
        m_prevS = s;
    }

    // If selected entitiy changed, calculate euler angles from quaternion
    DirectX::XMFLOAT3 GetEulerCache(bool selectionChanged)
    {
        if (selectionChanged)
            m_eulerCache = QuaternionToEuler(m_currR);

        return m_eulerCache;
    }

    // Convert euler angles to quaternion
    void SetRotation(const DirectX::XMFLOAT3& eulerRDeg)
    {
        m_eulerCache = eulerRDeg;

        float pitch = DirectX::XMConvertToRadians(eulerRDeg.x);
        float yaw = DirectX::XMConvertToRadians(eulerRDeg.y);
        float roll = DirectX::XMConvertToRadians(eulerRDeg.z);

        DirectX::XMVECTOR r = DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        DirectX::XMStoreFloat4(&m_currR, r);
        DirectX::XMStoreFloat4(&m_prevR, r);
    }

    DirectX::XMFLOAT3 GetTranslation() const
    {
        return m_currT;
    }

    void SetTranslation(const DirectX::XMFLOAT3& t)
    {
        m_currT = t;
        m_prevT = t;
    }

    // Assume that order of rotation is roll -> pitch -> yaw
    DirectX::XMFLOAT3 QuaternionToEuler(const DirectX::XMFLOAT4& q) const
    {
        // pitch φ (X): M_32 = -sinφ
        float sinPhi = 2.0f * (q.w * q.x - q.y * q.z); // 2(wx - yz)
        float pitch = DirectX::XMConvertToDegrees(
            std::fabsf(sinPhi) >= 1.0f ? std::copysignf(DirectX::XM_PIDIV2, sinPhi) : std::asinf(sinPhi));

        // yaw θ (Y): atan2(M_31, M_33)
        float yaw = DirectX::XMConvertToDegrees(
            std::atan2f(2.0f * (q.x * q.z + q.w * q.y),
                        1.0f - 2.0f * (q.x * q.x + q.y * q.y)));

        // roll ψ (Z): atan2(M_12, M_22)
        float roll = DirectX::XMConvertToDegrees(
            std::atan2f(2.0f * (q.x * q.y + q.w * q.z),
                        1.0f - 2.0f * (q.x * q.x + q.z * q.z)));

        return {pitch, yaw, roll};
    }

private:
    DirectX::XMFLOAT3 m_prevS, m_currS;
    DirectX::XMFLOAT4 m_prevR, m_currR;
    DirectX::XMFLOAT3 m_prevT, m_currT;

    DirectX::XMFLOAT4X4 m_localRenderTransform;
    DirectX::XMFLOAT4X4 m_worldRenderTransform;

    DirectX::XMFLOAT3 m_eulerCache;
};
