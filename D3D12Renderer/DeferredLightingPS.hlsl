#include "SharedConfig.h"

// SRV for lights
Texture2DArray<float> g_directionalShadowMaps[] : register(t0, space1);
TextureCube<float> g_PointShadowMaps[] : register(t0, space2);
Texture2D<float> g_SpotShadowMaps[] : register(t0, space3);

// GBuffers & depth buffer
Texture2D g_albedo : register(t0, space4);
Texture2D g_normal : register(t1, space4);
Texture2D g_materialAmbient : register(t2, space4);
Texture2D g_materialSpecular : register(t3, space4);
Texture2D g_depthBuffer : register(t4, space4);

// Static comparison samplers for shadow mapping
SamplerComparisonState g_comparisonSampler0 : register(s0, space1);
SamplerComparisonState g_comparisonSampler1 : register(s1, space1);

static const float2 vogelDisk[16] =
{
    float2(-0.1328, 0.1651), float2(0.3341, 0.0735), float2(-0.4042, -0.3150), float2(0.5055, -0.4124),
    float2(-0.1985, 0.5855), float2(0.1245, -0.7340), float2(-0.6401, 0.4578), float2(0.8123, 0.1901),
    float2(-0.6254, -0.6654), float2(0.1254, 0.9412), float2(0.4512, -0.8521), float2(-0.9254, 0.1254),
    float2(0.8521, 0.4512), float2(-0.4512, -0.9254), float2(0.1254, -0.1254), float2(0.9412, -0.1254)
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

cbuffer CameraConstantBuffer : register(b0, space0)
{
    float3 cameraPos;
    float4x4 view;
    float4x4 projection;
    float4x4 invView;
    float4x4 invProj;
}

cbuffer ShadowConstantBuffer : register(b1, space0)
{
    float cascadeSplits[MAX_CASCADES];
}

cbuffer GlobalConstants : register(b2, space0)
{
    uint numLights;
};

struct LightConstants
{
    float3 lightPos;
    float range;
    float3 lightDir;
    float cosOuterAngle;
    float3 lightColor;
    float cosInnerAngle;
    float4x4 viewProjection[POINT_LIGHT_ARRAY_SIZE];
    uint type;
    uint idxInArray;
    float lightIntensity;
};
ConstantBuffer<LightConstants> LightConstantBuffers[] : register(b0, space2);

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

// https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float InterleavedGradientNoise(float2 pixPos)
{
    return frac(52.9829189f * frac(dot(pixPos, float2(0.06711056f, 0.00583715f))));
}

// Shading in world space
// TODO : ambient add multiple times, fix this.
float3 PhongReflection(LightConstants light, float3 toLightWorld, float3 toCameraWorld, float lightFactor, float3 texColor, float3 normalWorld,
    float3 materialAmbient, float3 materialSpecular, float shininess)
{
    float3 halfWay = normalize(toLightWorld + toCameraWorld);
    
    // Ambient
    float3 ambient = materialAmbient * texColor;
    
    // Diffuse
    float nDotL = max(dot(normalWorld, toLightWorld), 0.0f);
    float3 diffuse = texColor * nDotL * light.lightColor * light.lightIntensity;
    
    // Specular
    float nDotH = max(dot(normalWorld, halfWay), 0.0f);
    float3 specular = pow(nDotH, shininess) * materialSpecular * light.lightColor * light.lightIntensity;
            
    return ambient + (diffuse + specular) * lightFactor;
}

float CalcAttenuation(float dist, float range)
{
    // Linear attenuation
    static const float fallOffStart = 0.4f;
    
    return saturate((range - dist) / (range - fallOffStart));
}

float CalcAngularAttenuation(LightConstants light, float3 lightToPixel)
{
    float cosTheta = dot(lightToPixel, light.lightDir);
    
    return saturate((cosTheta - light.cosOuterAngle) / (light.cosInnerAngle - light.cosOuterAngle));
}

float4 main(PSInput input) : SV_TARGET
{
    // Sample GBuffers
    int2 texCoord = int2(input.pos.xy);
    
    float3 texColor = g_albedo.Load(int3(texCoord, 0)).rgb;
    float4 temp = g_normal.Load(int3(texCoord, 0));
    float3 normalWorld = temp.xyz;
    float shininess = temp.w;
    float3 materialAmbient = g_materialAmbient.Load(int3(texCoord, 0)).rgb;
    float3 materialSpecular = g_materialSpecular.Load(int3(texCoord, 0)).rgb;
    
    // reconstruct world coordinate
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