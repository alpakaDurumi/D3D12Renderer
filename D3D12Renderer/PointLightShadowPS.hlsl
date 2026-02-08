#include "SharedConfig.h"

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1; // Do not interpolate w component of tangent vector.
    float distView : TEXCOORD2; // Distance in view space for determining CSM index.
};

struct LightConstants
{
    float3 lightPos;
    float range;
    float3 lightDir;
    float angle;
    float3 lightColor;
    float lightIntensity;
    float4x4 viewProjection[POINT_LIGHT_ARRAY_SIZE];
    uint type;
    uint idxInArray;
};
ConstantBuffer<LightConstants> LightConstantBuffers[] : register(b0, space1);

cbuffer IdxConstant : register(b4, space0)
{
    uint currentLightIdx;
};

// Render linear distance.
float4 main(PSInput input) : SV_TARGET
{
    float dist = distance(input.posWorld, LightConstantBuffers[currentLightIdx].lightPos);
    float normalizedDist = dist / LightConstantBuffers[currentLightIdx].range;
    
    return float4(normalizedDist, 0.0f, 0.0f, 1.0f);
}