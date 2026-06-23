#include "SharedConfig.h"
#include "POM.hlsli"
#include "Shadow.hlsli"
#include "Lighting.hlsli"
#include "LightConstants.hlsli"
#include "PSInput.hlsli"

Texture2D g_textures[] : register(t0, space1);

// Dynamic samplers for ordinary texture sampling
SamplerState g_samplers[] : register(s0, space0);

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

struct MaterialConstants
{
    float3 materialAmbient;
    float3 materialSpecular;
    float shininess;
    uint4 textureIndices;
    uint4 samplerIndices;
    float4 textureTileScales;
};
ConstantBuffer<MaterialConstants> MaterialConstantBuffers[] : register(b0, space1);

float4 main(MeshPSInput input) : SV_TARGET
{
    uint materialIdx = input.materialIndex;
    
    uint4 textureIndices = MaterialConstantBuffers[materialIdx].textureIndices;
    uint albedoIdx = textureIndices[0];
    uint normalMapIdx = textureIndices[1];
    uint heightMapIdx = textureIndices[2];
    
    uint4 samplerIndices = MaterialConstantBuffers[materialIdx].samplerIndices;
    uint albedoSamplerIdx = samplerIndices[0];
    uint normalMapSamplerIdx = samplerIndices[1];
    uint heightMapSamplerIdx = samplerIndices[2];
    
    float4 textureTileScales = MaterialConstantBuffers[materialIdx].textureTileScales;
    float albedoScale = textureTileScales[0];
    float normalMapScale = textureTileScales[1];
    float heightMapScale = textureTileScales[2];
    
    float3 materialAmbient = MaterialConstantBuffers[materialIdx].materialAmbient;
    float3 materialSpecular = MaterialConstantBuffers[materialIdx].materialSpecular;
    float shininess = MaterialConstantBuffers[materialIdx].shininess;
    
    // For POM, use inaccurate inverse-TBN
    float3 iT = normalize(input.tangentWorld);
    float3 iN = normalize(input.normalWorld);
    iT = normalize(iT - dot(iT, iN) * iN);              // Gram-Schmidt process
    float3 iB = input.tangentW * cross(iN, iT);         // W represents handedness
    float3x3 iiTBN = transpose(float3x3(iT, iB, iN));   // Inverse of orthogonal matrix is same as transpose.
    
    float3 toCameraWorld = normalize(cameraPos - input.posWorld);
    float3 toCameraTangent = normalize(mul(toCameraWorld, iiTBN));
    
    float2 texCoord = ParallaxMapping(input.texCoord * heightMapScale, toCameraTangent, g_textures[heightMapIdx], g_samplers[heightMapSamplerIdx]);
    float2 albedoTexCoord = texCoord * albedoScale / heightMapScale;
    float2 normalMapTexCoord = texCoord * normalMapScale / heightMapScale;
    
    //// Clip if texCoord exceeds boundary
    //if (texCoord.x < 0.0 || texCoord.x > 1.0 * textureTileScale || texCoord.y < 0.0 || texCoord.y > 1.0 * textureTileScale)
    //{
    //    clip(-1);
    //}
    
    // Set up TBN matrix
    float3 B = input.tangentW * cross(input.normalWorld, input.tangentWorld);
    float3x3 TBN = float3x3(input.tangentWorld, B, input.normalWorld);
    
    // Sample textures
    float3 texColor = g_textures[albedoIdx].Sample(g_samplers[albedoSamplerIdx], albedoTexCoord).rgb;
    float3 normal = g_textures[normalMapIdx].Sample(g_samplers[normalMapSamplerIdx], normalMapTexCoord).rgb * 2.0f - 1.0f;
    float3 normalWorld = normalize(mul(normal, TBN));

    uint csmIdx;
    float alpha;
    CalcCSMIndex(input.pos.w, csmIdx, alpha);   // SV_POSITION.w means view space distance.
    
    // Check CSM boundaries
    //if (csmIdx == 0)
    //{
    //    return lerp(float4(1.0f, 0.0f, 0.0f, 1.0f), float4(0.0f, 1.0f, 0.0f, 1.0f), alpha);
    //}
    //else if (csmIdx == 1)
    //{
    //    return lerp(float4(0.0f, 1.0f, 0.0f, 1.0f), float4(0.0f, 0.0f, 1.0f, 1.0f), alpha);
    //}
    //else if (csmIdx == 2)
    //{
    //    return lerp(float4(0.0f, 0.0f, 1.0f, 1.0f), float4(1.0f, 0.0f, 1.0f, 1.0f), alpha);
    //}
    //else if (csmIdx == 3)
    //{
    //    return lerp(float4(1.0f, 0.0f, 1.0f, 1.0f), float4(0.0f, 0.0f, 0.0f, 1.0f), alpha);
    //}
    //else
    //{
    //    return float4(0.0f, 0.0f, 0.0f, 1.0f);
    //}
    
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
                float4 lightScreen = mul(float4(input.posWorld, 1.0f), light.viewProjection[csmIdx]);
                lightScreen.xyz /= lightScreen.w;
                float2 lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
        
                shadowFactor = PCFDirectional(light.idxInArray, csmIdx, filterSize, lightTexCoord, lightScreen.z, rot);
            }
        
            // Second cascade. Only apply when overlapping can occur.
            if (csmIdx < MAX_CASCADES - 1)
            {
                float4 lightScreen = mul(float4(input.posWorld, 1.0f), light.viewProjection[csmIdx + 1]);
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
            float dist = distance(light.lightPos, input.posWorld);
            float normalizedDist = dist / light.range;
            
            float3 toLightWorld = normalize(light.lightPos - input.posWorld);
            
            float factor = CalcAttenuation(dist, light.range) *
                PCFPoint(light.idxInArray, filterSize, -toLightWorld, normalizedDist, rot);
            
            total += PhongReflection(light, toLightWorld, toCameraWorld, factor, texColor, normalWorld, materialAmbient, materialSpecular, shininess);
        }
        // Spot
        else if (light.type == LIGHT_TYPE_SPOT)
        {
            float3 toLightWorld = normalize(light.lightPos - input.posWorld);
            float dist = distance(light.lightPos, input.posWorld);

            float distAtt = CalcAttenuation(dist, light.range);
            float angularAtt = CalcAngularAttenuation(light, -toLightWorld);
            
            float4 lightScreen = mul(float4(input.posWorld, 1.0f), light.viewProjection[0]);
            lightScreen.xyz /= lightScreen.w;
            float2 lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
            
            float factor = distAtt * angularAtt * PCFSpot(light.idxInArray, filterSize, lightTexCoord, lightScreen.z, rot);
            
            total += PhongReflection(light, toLightWorld, toCameraWorld, factor, texColor, normalWorld, materialAmbient, materialSpecular, shininess);
        }
    }
    
    return float4(total, 1.0f);
}
