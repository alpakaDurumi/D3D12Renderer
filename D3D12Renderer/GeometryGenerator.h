#pragma once

#include <basetsd.h>
#include <minwindef.h>

#include <vector>

#include <DirectXMath.h>

#include "GeometryData.h"

class GeometryGenerator
{
public:
    static GeometryData GenerateCube()
    {
        std::vector<Vertex> vertices = {
            // upper
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},

            // lower
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},
            {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}},

            // left
            {{-1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},
            {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f}},

            // right
            {{1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},
            {{1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},

            // front
            {{-1.0f, -1.0f, -1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{-1.0f, 1.0f, -1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{1.0f, 1.0f, -1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},
            {{1.0f, -1.0f, -1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}},

            // back
            {{1.0f, -1.0f, 1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{1.0f, 1.0f, 1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
            {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}};

        std::vector<UINT32> indices = {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            8, 9, 10, 8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23};

        return {"builtin://mesh/cube", vertices, indices};
    }

    static GeometryData GenerateSphere()
    {
        const float radius = 1.0f;
        const UINT numStacks = 30;
        const UINT numSectors = 30;

        std::vector<Vertex> vertices;
        const float dPhi = DirectX::XM_PI / float(numStacks);
        const float dTheta = DirectX::XM_2PI / float(numSectors);
        for (UINT i = 0; i <= numStacks; i++)
        {
            float sinPhi, cosPhi;
            DirectX::XMScalarSinCos(&sinPhi, &cosPhi, i * dPhi);
            // Detect north pole and set to 0.0f for eliminating floating point error
            DirectX::XMFLOAT3 stackStart(0.0f, -radius * cosPhi, i == numStacks ? 0.0f : -radius * sinPhi);
            for (UINT j = 0; j <= numSectors; j++)
            {
                float sinTheta, cosTheta;
                DirectX::XMScalarSinCos(&sinTheta, &cosTheta, j * dTheta);

                Vertex v;
                v.position.x = -stackStart.z * sinTheta;
                v.position.y = stackStart.y;
                v.position.z = -stackStart.z * -cosTheta;
                v.texCoord = {float(j) / numSectors, 1.0f - float(i) / numStacks};
                if (stackStart.z == 0.0f) // Detect pole
                {
                    v.tangent = {1.0f, 0.0f, 0.0f, 1.0f};
                }
                else
                {
                    DirectX::XMStoreFloat4(&v.tangent, DirectX::XMVector3Normalize(DirectX::XMVectorSet(-stackStart.z * cosTheta, 0.0f, -stackStart.z * sinTheta, 0.0f)));
                    v.tangent.w = 1.0f;
                }
                DirectX::XMStoreFloat3(&v.normal, DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&v.position)));

                vertices.push_back(v);
            }
        }

        std::vector<UINT32> indices;
        for (UINT i = 0; i < numStacks; i++)
        {
            for (UINT j = 0; j < numSectors; j++)
            {
                UINT p1 = i * (numSectors + 1) + j;
                UINT p2 = (i + 1) * (numSectors + 1) + j;
                UINT p3 = p2 + 1;
                UINT p4 = p1 + 1;
                indices.push_back(p1);
                indices.push_back(p2);
                indices.push_back(p3);
                indices.push_back(p1);
                indices.push_back(p3);
                indices.push_back(p4);
            }
        }

        return {"builtin://mesh/sphere", vertices, indices};
    }
};
