#include "SharedConfig.h"
#include "Shadow.hlsli"
#include "Lighting.hlsli"
#include "LightConstants.hlsli"
#include "PSInput.hlsli"
#include "SurfaceData.hlsli"
#include "CameraConstants.hlsli"

cbuffer GlobalConstants : register(b2, space0)
{
    uint numLights;
};

float4 main(MeshPSInput input) : SV_TARGET
{
    SurfaceData data = CalcSurfaceData(input, cameraPos);
    
    float3 toCameraWorld = normalize(cameraPos - input.posWorld);

    uint csmIdx;
    float alpha;
    CalcCSMIndex(input.pos.w, csmIdx, alpha); // SV_POSITION.w means view space distance.
    
    // Pass random rotation to PCF based on IGN
    float noise = InterleavedGradientNoise(input.pos.xy);
    float angle = noise * 2.0f * 3.141592f;
    float2x2 rot = float2x2(cos(angle), -sin(angle), sin(angle), cos(angle));
    
    static const float filterSize = 5.0f;
    
    float3 total = float3(0.0f, 0.0f, 0.0f);
    
    [loop]
    for (uint i = 0; i < numLights; ++i)
    {
        LightConstants light = LightConstantBuffers[i];
        
        float shadowFactor;
        
        // Directional
        if (light.type == LIGHT_TYPE_DIRECTIONAL)
        {
            // First cascade
            {
                float2 lightTexCoord;
                float z = WorldToShadowUV(input.posWorld, light.viewProjection[csmIdx], lightTexCoord);
        
                shadowFactor = PCFDirectional(light.idxInArray, csmIdx, filterSize, lightTexCoord, z, rot);
            }
        
            // Second cascade. Only apply when overlapping can occur.
            if (csmIdx < MAX_CASCADES - 1)
            {
                float2 lightTexCoord;
                float z = WorldToShadowUV(input.posWorld, light.viewProjection[csmIdx + 1], lightTexCoord);
        
                float t = PCFDirectional(light.idxInArray, csmIdx + 1, filterSize, lightTexCoord, z, rot);
                shadowFactor = lerp(shadowFactor, t, alpha);
            }
            
            // Shading in world space
            float3 toLightWorld = -light.lightDir;
            total += PhongReflection(light, toLightWorld, toCameraWorld, shadowFactor, data.albedo.rgb, data.normalWorld, data.ambient.rgb, data.specular.rgb, data.shininess);
        }
        // Point
        else if (light.type == LIGHT_TYPE_POINT)
        {
            float dist = distance(light.lightPos, input.posWorld);
            float normalizedDist = dist / light.range;
            
            float3 toLightWorld = normalize(light.lightPos - input.posWorld);
            
            float factor = CalcAttenuation(dist, light.range) * PCFPoint(light.idxInArray, filterSize, -toLightWorld, normalizedDist, rot);
            
            total += PhongReflection(light, toLightWorld, toCameraWorld, factor, data.albedo.rgb, data.normalWorld, data.ambient.rgb, data.specular.rgb, data.shininess);
        }
        // Spot
        else if (light.type == LIGHT_TYPE_SPOT)
        {
            float3 toLightWorld = normalize(light.lightPos - input.posWorld);
            float dist = distance(light.lightPos, input.posWorld);

            float distAtt = CalcAttenuation(dist, light.range);
            float angularAtt = CalcAngularAttenuation(light, -toLightWorld);
            
            float2 lightTexCoord;
            float z = WorldToShadowUV(input.posWorld, light.viewProjection[0], lightTexCoord);
            
            float factor = distAtt * angularAtt * PCFSpot(light.idxInArray, filterSize, lightTexCoord, z, rot);
            
            total += PhongReflection(light, toLightWorld, toCameraWorld, factor, data.albedo.rgb, data.normalWorld, data.ambient.rgb, data.specular.rgb, data.shininess);
        }
    }
    
    return float4(total, 1.0f);
}
