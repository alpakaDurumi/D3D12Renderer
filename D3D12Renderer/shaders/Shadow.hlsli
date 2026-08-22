#pragma once

#include "../SharedConfig.h"

// SRV for lights
Texture2DArray<float> g_directionalShadowMaps[] : register(t0, space2);
TextureCube<float> g_PointShadowMaps[] : register(t0, space3);
Texture2D<float> g_SpotShadowMaps[] : register(t0, space4);

// Static comparison samplers for shadow mapping
SamplerComparisonState g_comparisonSampler0 : register(s0, space1);
SamplerComparisonState g_comparisonSampler1 : register(s1, space1);

cbuffer ShadowConstantBuffer : register(b1, space0)
{
    float cascadeSplits[MAX_CASCADES];
}

static const float2 vogelDisk[16] =
{
    float2(-0.1328, 0.1651), float2(0.3341, 0.0735), float2(-0.4042, -0.3150), float2(0.5055, -0.4124),
    float2(-0.1985, 0.5855), float2(0.1245, -0.7340), float2(-0.6401, 0.4578), float2(0.8123, 0.1901),
    float2(-0.6254, -0.6654), float2(0.1254, 0.9412), float2(0.4512, -0.8521), float2(-0.9254, 0.1254),
    float2(0.8521, 0.4512), float2(-0.4512, -0.9254), float2(0.1254, -0.1254), float2(0.9412, -0.1254)
};

// https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float InterleavedGradientNoise(float2 pixPos)
{
    return frac(52.9829189f * frac(dot(pixPos, float2(0.06711056f, 0.00583715f))));
}

// Determine which index to use and alpha for interpolation.
void CalcCSMIndex(float distView, out uint index, out float alpha)
{
    static const float overlapScale = 0.1f;
    float overlap;
    for (index = 0; index < MAX_CASCADES - 1; ++index)
    {
        overlap = (cascadeSplits[index + 1] - cascadeSplits[index]) * overlapScale;
        if (distView < cascadeSplits[index] + overlap)
        {
            break;
        }
    }
    
    alpha = smoothstep(cascadeSplits[index] - overlap, cascadeSplits[index] + overlap, distView);
}

// Return clip-space depth, output shadowMap UV
float WorldToShadowUV(float3 posWorld, float4x4 viewProjection, out float2 lightTexCoord)
{
    float4 lightScreen = mul(float4(posWorld, 1.0f), viewProjection);
    lightScreen.xyz /= lightScreen.w;
    lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
    return lightScreen.z;
}

float PCFDirectional(uint idxInArray, uint csmIdx, float filterSize, float2 texCoord, float compareValue, float2x2 rot)
{
    float shadowFactor = 0.0f;
        
    uint width, height, elements;
    g_directionalShadowMaps[idxInArray].GetDimensions(width, height, elements);
    float dx = filterSize / width;
        
    [unroll]
    for (uint j = 0; j < 16; ++j)
    {
        float2 rotated = mul(vogelDisk[j], rot);
        float2 offset = rotated * dx;
        shadowFactor += g_directionalShadowMaps[idxInArray].SampleCmpLevelZero(g_comparisonSampler0, float3(texCoord + offset, float(csmIdx)), compareValue);
    }
    
    shadowFactor /= 16.0f;
    
    return shadowFactor;
}

float PCFPoint(uint idxInArray, float filterSize, float3 lightToPixel, float compareValue, float2x2 rot)
{
    float shadowFactor = 0.0f;
    
    uint width, height;
    g_PointShadowMaps[idxInArray].GetDimensions(width, height);
    float dx = filterSize / width;
    
    float3 up = abs(lightToPixel.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, -1.0f);
    float3 T = normalize(cross(up, lightToPixel));
    float3 B = cross(T, lightToPixel);
    
    [unroll]
    for (uint j = 0; j < 16; ++j)
    {
        float2 rotated = mul(vogelDisk[j], rot);
        float3 offset = (T * rotated.x + B * rotated.y) * dx;
        shadowFactor += g_PointShadowMaps[idxInArray].SampleCmpLevelZero(g_comparisonSampler1, lightToPixel + offset, compareValue);
    }
    
    shadowFactor /= 16.0f;
    
    return shadowFactor;
}

float PCFSpot(uint idxInArray, float filterSize, float2 texCoord, float compareValue, float2x2 rot)
{
    float shadowFactor = 0.0f;
        
    uint width, height;
    g_SpotShadowMaps[idxInArray].GetDimensions(width, height);
    float dx = filterSize / width;
        
    [unroll]
    for (uint j = 0; j < 16; ++j)
    {
        float2 rotated = mul(vogelDisk[j], rot);
        float2 offset = rotated * dx;
        shadowFactor += g_SpotShadowMaps[idxInArray].SampleCmpLevelZero(g_comparisonSampler0, texCoord + offset, compareValue);
    }
    
    shadowFactor /= 16.0f;
    
    return shadowFactor;
}
