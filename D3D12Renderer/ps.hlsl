#include "SharedConfig.h"

Texture2D g_textures[] : register(t0, space0);

// SRV for lights
Texture2DArray<float> g_directionalShadowMaps[] : register(t0, space1);
TextureCube<float> g_PointShadowMaps[] : register(t0, space2);
Texture2D<float> g_SpotShadowMaps[] : register(t0, space3);

// Dynamic samplers for ordinary texture sampling
SamplerState g_samplers[] : register(s0, space0);

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
    float3 posWorld : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1;     // Do not interpolate w component of tangent vector.
    float distView : TEXCOORD2;                     // Distance in view space for determining CSM index.
};

cbuffer MeshConstantBuffer : register(b0, space0)
{
    float4x4 world;
    float4x4 inverseTranspose;
    float textureTileScale;
};

cbuffer CameraConstantBuffer : register(b1, space0)
{
    float3 cameraPos;
    float4x4 view;
    float4x4 projection;
}

cbuffer MaterialConstantBuffer : register(b2, space0)
{
    float3 materialAmbient;
    float3 materialSpecular;
    float shininess;
    uint4 textureIndices;
}

cbuffer ShadowConstantBuffer : register(b3, space0)
{
    float cascadeSplits[MAX_CASCADES];
}

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
ConstantBuffer<LightConstants> LightConstantBuffers[] : register(b0, space1);

cbuffer GlobalConstants : register(b5, space0)
{
    uint samplerIdx;
    uint numLights;
};

// Parallax Occlusion Mapping
float2 ParallaxMapping(float2 texCoord, float3 toCamera)
{
    static const float heightScale = 0.02f;
    
    // Set numLayers based on view direction
    static const float minLayers = 8.0f;
    static const float maxLayers = 32.0f;
    const float numLayers = lerp(maxLayers, minLayers, abs(dot(float3(0.0f, 0.0f, 1.0f), toCamera)));
    
    float layerStep = 1.0f / numLayers;
    
    // Texture coord offset per layer
    // xy / z = offset / (1.0 * heightScale)
    float2 deltaTexCoord = toCamera.xy / max(toCamera.z, 0.001f) * heightScale / numLayers;
    
    float2 currentTexCoord = texCoord * textureTileScale;
    float2 dx = ddx(currentTexCoord);
    float2 dy = ddy(currentTexCoord);
    float currentHeightMapValue = 1.0f - g_textures[textureIndices[2]].SampleGrad(g_samplers[samplerIdx], currentTexCoord, dx, dy).r;
    float currentLayerHeight = 0.0f;
    
    float2 prevTexCoord = currentTexCoord;
    float prevHeightMapValue = currentHeightMapValue;
    float prevLayerHeight = currentLayerHeight;
    
    [loop]
    while (currentLayerHeight < 1.0f && currentLayerHeight < currentHeightMapValue)
    {
        prevTexCoord = currentTexCoord;
        prevHeightMapValue = currentHeightMapValue;
        prevLayerHeight = currentLayerHeight;
        
        currentTexCoord -= deltaTexCoord;
        currentHeightMapValue = 1.0f - g_textures[textureIndices[2]].SampleGrad(g_samplers[samplerIdx], currentTexCoord, dx, dy).r;
        currentLayerHeight += layerStep;
    }
    
    // Interpolate
    float diffAfter = currentLayerHeight - currentHeightMapValue;
    float diffBefore = prevHeightMapValue - prevLayerHeight;
    
    // Eliminate divide by zero
    float weight = saturate(diffBefore / max(diffAfter + diffBefore, 0.00001f));
    float2 interpolatedTexCoord = lerp(prevTexCoord, currentTexCoord, weight);
    
    return interpolatedTexCoord;
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

// https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float InterleavedGradientNoise(float2 pixPos)
{
    return frac(52.9829189f * frac(dot(pixPos, float2(0.06711056f, 0.00583715f))));
}

// Shading in world space
// TODO : ambient add multiple times, fix this.
float3 PhongReflection(LightConstants light, float3 toLightWorld, float3 toCameraWorld, float lightFactor, float3 texColor, float3 normalWorld)
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

float4 main(PSInput input) : SV_TARGET
{
    // For POM, use inaccurate inverse-TBN
    float3 iT = normalize(input.tangentWorld);
    float3 iN = normalize(input.normalWorld);
    iT = normalize(iT - dot(iT, iN) * iN);              // Gram-Schmidt process
    float3 iB = input.tangentW * cross(iN, iT);         // W represents handedness
    float3x3 iiTBN = transpose(float3x3(iT, iB, iN));   // Inverse of orthogonal matrix is same as transpose.
    
    float3 toCameraWorld = normalize(cameraPos - input.posWorld);
    float3 toCameraTangent = normalize(mul(toCameraWorld, iiTBN));
    
    float2 texCoord = ParallaxMapping(input.texCoord, toCameraTangent);

    //// Clip if texCoord exceeds boundary
    //if (texCoord.x < 0.0 || texCoord.x > 1.0 * textureTileScale || texCoord.y < 0.0 || texCoord.y > 1.0 * textureTileScale)
    //{
    //    clip(-1);
    //}
    
    // Set up TBN matrix
    float3 B = input.tangentW * cross(input.normalWorld, input.tangentWorld);
    float3x3 TBN = float3x3(input.tangentWorld, B, input.normalWorld);
    
    // Sample textures
    float3 texColor = g_textures[textureIndices[0]].Sample(g_samplers[samplerIdx], texCoord).rgb;
    float3 normal = g_textures[textureIndices[1]].Sample(g_samplers[samplerIdx], texCoord).rgb * 2.0f - 1.0f;
    float3 normalWorld = normalize(mul(normal, TBN));

    uint csmIdx;
    float alpha;
    CalcCSMIndex(input.distView, csmIdx, alpha);
    
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
            total += PhongReflection(light, toLightWorld, toCameraWorld, shadowFactor, texColor, normalWorld);
        }
        // Point
        else if (light.type == LIGHT_TYPE_POINT)
        {
            float dist = distance(light.lightPos, input.posWorld);
            float normalizedDist = dist / light.range;
            
            float3 toLightWorld = normalize(light.lightPos - input.posWorld);
            
            float factor = CalcAttenuation(dist, light.range) *
                PCFPoint(light.idxInArray, filterSize, -toLightWorld, normalizedDist, rot);
            
            total += PhongReflection(light, toLightWorld, toCameraWorld, factor, texColor, normalWorld);
        }
        // Spot
        else if (light.type == LIGHT_TYPE_SPOT)
        {
            float3 toLightWorld = normalize(light.lightPos - input.posWorld);
            float cosTheta = dot(-toLightWorld, light.lightDir);
            
            float dist = distance(light.lightPos, input.posWorld);

            float distAtt = CalcAttenuation(dist, light.range);
            float angularAttenuation = saturate((cosTheta - light.cosOuterAngle) / (light.cosInnerAngle - light.cosOuterAngle));
            
            total += PhongReflection(light, toLightWorld, toCameraWorld, distAtt * angularAttenuation, texColor, normalWorld);
        }
    }
    
    return float4(total, 1.0f);
}