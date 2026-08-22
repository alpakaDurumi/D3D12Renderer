#pragma once

#include "../SharedConfig.h"

struct LightConstants
{
    float3 lightPos;
    float range;
    float3 lightDir;
    float cosOuterAngle;
    float3 lightColor;
    float cosInnerAngle;
    float4x4 viewProjection[MAX_CASCADES];
    uint type;
    uint idxInArray;
    float lightIntensity;
};

ConstantBuffer<LightConstants> LightConstantBuffers[] : register(b0, space2);
