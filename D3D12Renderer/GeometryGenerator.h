#pragma once

#include "Windows.h"

#include <DirectXMath.h>

#include <vector>

using namespace DirectX;

struct Vertex
{
    XMFLOAT3 position;
    XMFLOAT2 texCoord;
    XMFLOAT4 tangent;
    XMFLOAT3 normal;
};

struct GeometryData
{
    std::vector<Vertex> vertices;
    std::vector<UINT32> indices;
};

struct InstanceData
{
    XMFLOAT4X4 world;
    XMFLOAT4X4 inverseTranspose;
};

class GeometryGenerator
{
public:
    static GeometryData GenerateCube()
    {
        std::vector<Vertex> vertices =
        {
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
            {{-1.0f, -1.0f, 1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}}
        };

        std::vector<UINT32> indices =
        {
            0, 1, 2, 0, 2, 3,
            4, 5, 6, 4, 6, 7,
            8, 9, 10, 8, 10, 11,
            12, 13, 14, 12, 14, 15,
            16, 17, 18, 16, 18, 19,
            20, 21, 22, 20, 22, 23
        };

        return { vertices, indices };
    }

    static GeometryData GenerateSphere()
    {
        const float radius = 1.0f;
        const UINT numStacks = 30;
        const UINT numSectors = 30;

        std::vector<Vertex> vertices;
        const float dPhi = XM_PI / float(numStacks);
        const float dTheta = XM_2PI / float(numSectors);
        for (UINT i = 0; i <= numStacks; i++)
        {
            float sinPhi, cosPhi;
            XMScalarSinCos(&sinPhi, &cosPhi, i * dPhi);
            // Detect north pole and set to 0.0f for eliminating floating point error
            XMFLOAT3 stackStart(0.0f, -radius * cosPhi, i == numStacks ? 0.0f : -radius * sinPhi);
            for (UINT j = 0; j <= numSectors; j++)
            {
                float sinTheta, cosTheta;
                XMScalarSinCos(&sinTheta, &cosTheta, j * dTheta);

                Vertex v;
                v.position.x = -stackStart.z * sinTheta;
                v.position.y = stackStart.y;
                v.position.z = -stackStart.z * -cosTheta;
                v.texCoord = { float(j) / numSectors, 1.0f - float(i) / numStacks };
                if (stackStart.z == 0.0f)       // Detect pole
                {
                    v.tangent = { 1.0f, 0.0f, 0.0f, 1.0f };
                }
                else
                {
                    XMStoreFloat4(&v.tangent, XMVector3Normalize(XMVectorSet(-stackStart.z * cosTheta, 0.0f, -stackStart.z * sinTheta, 0.0f)));
                    v.tangent.w = 1.0f;
                }
                XMStoreFloat3(&v.normal, XMVector3Normalize(XMLoadFloat3(&v.position)));

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

        return { vertices, indices };
    }

    static std::vector<InstanceData> GenerateSampleInstanceData()
    {
        GeometryData t = GenerateCube();

        std::vector<InstanceData> instanceData;

        for (int i = 0; i < 100; i++)
        {
            for (int j = 0; j < 100; j++)
            {
                InstanceData data;
                XMMATRIX world = XMMatrixTranslation((i - 50.0f) * 4.0f, (j - 50.0f) * 4.0f, 10.0f);
                XMStoreFloat4x4(&data.world, XMMatrixTranspose(world));
                world.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
                XMStoreFloat4x4(&data.inverseTranspose, XMMatrixInverse(nullptr, world));
                instanceData.push_back(data);
            }
        }

        return instanceData;
    }
};