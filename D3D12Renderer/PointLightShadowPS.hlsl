#include "PSInput.hlsli"
#include "SharedConfig.h"
#include "LightConstants.hlsli"

cbuffer IdxConstant : register(b3, space0)
{
    uint currentLightIdx;
};

// Render linear distance.
float4 main(MeshPSInput input) : SV_TARGET
{
    float dist = distance(input.posWorld, LightConstantBuffers[currentLightIdx].lightPos);
    float normalizedDist = dist / LightConstantBuffers[currentLightIdx].range;
    
    // Software slope-scaled bias
    // In original depth bias caculating logic : Bias = (float)DepthBias * 2**(exponent(max z in primitive) - r) + SlopeScaledDepthBias * MaxDepthSlope;
    // the term '2**(exponent(max z in primitive) - r)' can be ignored in here because we're using values in linear space.
    static const float constBias = 0.0002f;
    static const float slopeScaledBias = 1.0f;
    static const float biasClamp = 0.01f;
    
    float bias = constBias + slopeScaledBias * max(abs(ddx(normalizedDist)), abs(ddy(normalizedDist)));
    bias = clamp(bias, 0.0f, biasClamp);
    
    return float4(normalizedDist + bias, 0.0f, 0.0f, 1.0f);
}
