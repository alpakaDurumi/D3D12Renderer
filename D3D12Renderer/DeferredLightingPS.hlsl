#include "SharedConfig.h"
#include "Shadow.hlsli"
#include "Lighting.hlsli"
#include "LightConstants.hlsli"
#include "PSInput.hlsli"

// GBuffers & depth buffer
Texture2D g_gBuffers[NUM_GBUFFER_SLOTS] : register(t0, space5);
Texture2D g_depthBuffer : register(t0, space6);

cbuffer CameraConstantBuffer : register(b0, space0)
{
    float3 cameraPos;
    float4x4 view;
    float4x4 projection;
    float4x4 invView;
    float4x4 invProj;
}

cbuffer GlobalConstants : register(b2, space0)
{
    uint numLights;
};

float4 main(FullScreenPSInput input) : SV_TARGET
{
    // Sample GBuffers
    int2 texCoord = int2(input.pos.xy);
    
    float3 texColor = g_gBuffers[GBUFFER_SLOT_ALBEDO].Load(int3(texCoord, 0)).rgb;
    float4 temp = g_gBuffers[GBUFFER_SLOT_NORMAL].Load(int3(texCoord, 0));
    float3 normalWorld = temp.xyz;
    float shininess = temp.w;
    float3 materialAmbient = g_gBuffers[GBUFFER_SLOT_MAT_AMBIENT].Load(int3(texCoord, 0)).rgb;
    float3 materialSpecular = g_gBuffers[GBUFFER_SLOT_MAT_SPECULAR].Load(int3(texCoord, 0)).rgb;
    
    // reconstruct world position
    float depth = g_depthBuffer.Load(int3(texCoord, 0)).r;
    float4 ndc = float4(input.texCoord.x * 2.0f - 1.0f, 1.0f - input.texCoord.y * 2.0f, depth, 1.0f);
    float4 posView = mul(ndc, invProj);
    posView /= posView.w;       // perspective division
    float3 posWorld = mul(posView, invView).xyz;
    
    float3 toCameraWorld = normalize(cameraPos - posWorld);
    
    uint csmIdx;
    float alpha;
    CalcCSMIndex(posView.z, csmIdx, alpha);
    
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
                float4 lightScreen = mul(float4(posWorld, 1.0f), light.viewProjection[csmIdx]);
                lightScreen.xyz /= lightScreen.w;
                float2 lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
        
                shadowFactor = PCFDirectional(light.idxInArray, csmIdx, filterSize, lightTexCoord, lightScreen.z, rot);
            }
        
            // Second cascade. Only apply when overlapping can occur.
            if (csmIdx < MAX_CASCADES - 1)
            {
                float4 lightScreen = mul(float4(posWorld, 1.0f), light.viewProjection[csmIdx + 1]);
                lightScreen.xyz /= lightScreen.w;
                float2 lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
        
                float t = PCFDirectional(light.idxInArray, csmIdx + 1, filterSize, lightTexCoord, lightScreen.z, rot);
                shadowFactor = lerp(shadowFactor, t, alpha);
            }
            
            // Shading in world space
            float3 toLightWorld = -light.lightDir;
            total += PhongReflection(light, toLightWorld, toCameraWorld, shadowFactor, texColor, normalWorld, materialAmbient, materialSpecular, shininess);
        }
        // Point
        else if (light.type == LIGHT_TYPE_POINT)
        {
            float dist = distance(light.lightPos, posWorld);
            float normalizedDist = dist / light.range;
            
            float3 toLightWorld = normalize(light.lightPos - posWorld);
            
            float factor = CalcAttenuation(dist, light.range) *
                PCFPoint(light.idxInArray, filterSize, -toLightWorld, normalizedDist, rot);
            
            total += PhongReflection(light, toLightWorld, toCameraWorld, factor, texColor, normalWorld, materialAmbient, materialSpecular, shininess);
        }
        // Spot
        else if (light.type == LIGHT_TYPE_SPOT)
        {
            float3 toLightWorld = normalize(light.lightPos - posWorld);
            float dist = distance(light.lightPos, posWorld);

            float distAtt = CalcAttenuation(dist, light.range);
            float angularAtt = CalcAngularAttenuation(light, -toLightWorld);
            
            float4 lightScreen = mul(float4(posWorld, 1.0f), light.viewProjection[0]);
            lightScreen.xyz /= lightScreen.w;
            float2 lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
            
            float factor = distAtt * angularAtt * PCFSpot(light.idxInArray, filterSize, lightTexCoord, lightScreen.z, rot);
            
            total += PhongReflection(light, toLightWorld, toCameraWorld, factor, texColor, normalWorld, materialAmbient, materialSpecular, shininess);
        }
    }
    
    return float4(total, 1.0f);
}
