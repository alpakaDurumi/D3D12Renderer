#pragma once

#include <DirectXMath.h>

#include "InstanceData.h"

using namespace DirectX;

class Material;
class Mesh;

class RenderObject
{
public:
    RenderObject(Mesh* pMesh);

    void SetInitialTransform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t);
    void SnapshotState();
    void Transform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t);
    void UpdateRenderState(float alpha);

    InstanceData BuildInstanceData();

    void SetMaterial(Material* mat);

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