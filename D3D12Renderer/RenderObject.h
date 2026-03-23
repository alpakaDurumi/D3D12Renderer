#pragma once

#include "Mesh.h"
#include "SharedConfig.h"

class RenderObject
{
public:
    RenderObject(Mesh* pMesh)
        : m_pMesh(pMesh)
    {
        m_pMaterial = pMesh->m_pDefaultMaterial;

        m_prevS = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_currS = XMFLOAT3(1.0f, 1.0f, 1.0f);
        m_prevR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_currR = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        m_prevT = XMFLOAT3(0.0f, 0.0f, 0.0f);
        m_currT = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }

    void SetInitialTransform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
    {
        m_prevS = m_currS = s;

        XMVECTOR r = XMQuaternionRotationRollPitchYaw(eulerRad.x, eulerRad.y, eulerRad.z);
        XMStoreFloat4(&m_prevR, r);
        m_currR = m_prevR;

        m_prevT = m_currT = t;
    }

    void SnapshotState()
    {
        m_prevS = m_currS;
        m_prevR = m_currR;
        m_prevT = m_currT;
    }

    // Accumulate each component
    void Transform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t)
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

    void UpdateRenderState(float alpha)
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

        XMStoreFloat4x4(&m_renderTransform, XMMatrixAffineTransformation(renderS, XMVectorZero(), renderR, renderT));
    }

    InstanceData BuildInstanceData()
    {
        InstanceData ret;
        
        auto world = XMLoadFloat4x4(&m_renderTransform);

        XMStoreFloat4x4(&ret.world, XMMatrixTranspose(world));

        world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        XMStoreFloat4x4(&ret.inverseTranspose, XMMatrixInverse(nullptr, world));

        ret.materialIndex = m_pMaterial->GetMaterialConstantBufferIndex();

        return ret;
    }

    void SetMaterial(Material* mat)
    {
        m_pMaterial = mat;
    }

private:
    Mesh* m_pMesh;

    XMFLOAT3 m_prevS, m_currS;
    XMFLOAT4 m_prevR, m_currR;
    XMFLOAT3 m_prevT, m_currT;

    XMFLOAT4X4 m_renderTransform;

    bool m_visible;

    // TODO:
    // material override
    Material* m_pMaterial = nullptr;
};