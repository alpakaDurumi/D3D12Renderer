#pragma once

#include <DirectXMath.h>

#include "Object.h"
#include "InstanceData.h"
#include "SceneHandles.h"

using namespace DirectX;

class RenderObject : public Object
{
public:
    RenderObject(MeshHandle mesh);

    void SetInitialTransform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t);
    void SnapshotState();
    void Transform(const XMFLOAT3& s, const XMFLOAT3& eulerRad, const XMFLOAT3& t);
    void UpdateRenderState(float alpha);

    InstanceData BuildInstanceData(UINT matIdx) const;

    MeshHandle GetMesh() const;

    MaterialHandle GetMaterial() const;
    void SetMaterial(MaterialHandle material);

private:
    XMFLOAT3 m_prevS, m_currS;
    XMFLOAT4 m_prevR, m_currR;
    XMFLOAT3 m_prevT, m_currT;

    XMFLOAT4X4 m_renderTransform;

    bool m_visible;

    // TODO:
    // material override
    MeshHandle m_mesh;
    MaterialHandle m_material;
};