#pragma once

struct MeshPSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
#ifndef DEPTH_ONLY
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1; // Do not interpolate w component of tangent vector.
    nointerpolation uint materialIndex : INSTANCE_MATERIAL_INDEX;
#endif  // DEPTH_ONLY
};

struct FullScreenPSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};
