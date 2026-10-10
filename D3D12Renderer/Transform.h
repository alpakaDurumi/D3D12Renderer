#pragma once

#include <DirectXMath.h>

class Transform
{
public:
    Transform();
    Transform(const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& eulerRad, const DirectX::XMFLOAT3& t);

    void Apply(const DirectX::XMFLOAT3& s, const DirectX::XMFLOAT3& eulerRad, const DirectX::XMFLOAT3& t);

    // render interpolation
    void SnapshotState()
    {
        m_prevS = m_currS;
        m_prevR = m_currR;
        m_prevT = m_currT;
    }

    void UpdateLocalRenderState(float alpha);

    const DirectX::XMFLOAT4X4& GetLocalRenderTransform() const
    {
        return m_localRenderTransform;
    }

    void XM_CALLCONV SetWorldRenderTransform(DirectX::FXMMATRIX transform)
    {
        DirectX::XMStoreFloat4x4(&m_worldRenderTransform, transform);
    }

    DirectX::XMFLOAT4X4 GetWorldRenderTransform() const
    {
        return m_worldRenderTransform;
    }

    // S/R/T
    DirectX::XMFLOAT3 GetScale() const
    {
        return m_currS;
    }

    void SetScale(const DirectX::XMFLOAT3& s)
    {
        m_currS = s;
        m_prevS = s;
    }

    DirectX::XMFLOAT3 GetEulerCache(bool selectionChanged);

    void SetRotation(const DirectX::XMFLOAT3& eulerRDeg);

    void SetForward(const DirectX::XMFLOAT3& forward);

    DirectX::XMFLOAT3 GetTranslation() const
    {
        return m_currT;
    }

    void SetTranslation(const DirectX::XMFLOAT3& t)
    {
        m_currT = t;
        m_prevT = t;
    }

private:
    DirectX::XMFLOAT3 m_prevS, m_currS;
    DirectX::XMFLOAT4 m_prevR, m_currR;
    DirectX::XMFLOAT3 m_prevT, m_currT;

    DirectX::XMFLOAT4X4 m_localRenderTransform;
    DirectX::XMFLOAT4X4 m_worldRenderTransform;

    DirectX::XMFLOAT3 m_eulerCache;
};
