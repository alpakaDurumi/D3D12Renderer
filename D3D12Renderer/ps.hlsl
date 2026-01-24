#include "SharedConfig.h"

Texture2D g_albedo : register(t0, space0);
Texture2D g_normalMap : register(t1, space0);
Texture2D g_heightMap : register(t2, space0);

Texture2DArray<float> g_shadowMaps[] : register(t0, space1);

SamplerState g_sampler : register(s0);
SamplerComparisonState g_samplerComparison : register(s1);

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
    float angle;
    float3 lightColor;
    float lightIntensity;
    float4x4 viewProjection[POINT_LIGHT_ARRAY_SIZE];
    uint type;
};
ConstantBuffer<LightConstants> LightConstantBuffers[] : register(b0, space1);

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
    float currentHeightMapValue = 1.0f - g_heightMap.SampleGrad(g_sampler, currentTexCoord, dx, dy).r;
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
        currentHeightMapValue = 1.0f - g_heightMap.SampleGrad(g_sampler, currentTexCoord, dx, dy).r;
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

float PCF(uint lightIdx, uint csmIdx, float filterSize, float2 texCoord, float compareValue, float2x2 rot)
{
    float shadowFactor = 0.0f;
        
    //static const float2 poissonDisk[16] =
    //{
    //    float2(-0.94201624, -0.39906216), float2(0.94558609, -0.76890725),
    //    float2(-0.094184101, -0.92938870), float2(0.34495938, 0.29387760),
    //    float2(-0.91588581, 0.45771432), float2(-0.81544232, -0.87912464),
    //    float2(-0.38277543, 0.27676845), float2(0.97484398, 0.75648379),
    //    float2(0.44323325, -0.97511554), float2(0.53742981, -0.47373420),
    //    float2(-0.26496911, -0.41893023), float2(0.79197514, 0.19090188),
    //    float2(-0.24188840, 0.99706507), float2(-0.81409955, 0.91437590),
    //    float2(0.19984126, 0.78641367), float2(0.14383161, -0.14100790)
    //};
        
    static const float2 vogelDisk[16] =
    {
        float2(-0.1328, 0.1651), float2(0.3341, 0.0735), float2(-0.4042, -0.3150), float2(0.5055, -0.4124),
        float2(-0.1985, 0.5855), float2(0.1245, -0.7340), float2(-0.6401, 0.4578), float2(0.8123, 0.1901),
        float2(-0.6254, -0.6654), float2(0.1254, 0.9412), float2(0.4512, -0.8521), float2(-0.9254, 0.1254),
        float2(0.8521, 0.4512), float2(-0.4512, -0.9254), float2(0.1254, -0.1254), float2(0.9412, -0.1254)
    };
    
    uint width, height, elements;
    g_shadowMaps[lightIdx].GetDimensions(width, height, elements);
    float dx = filterSize / width;
        
    [unroll]
    for (uint j = 0; j < 16; ++j)
    {
        float2 rotated = mul(vogelDisk[j], rot);
        shadowFactor += g_shadowMaps[lightIdx].SampleCmpLevelZero(g_samplerComparison, float3(texCoord + rotated * dx, float(csmIdx)), compareValue);
    }
    
    shadowFactor /= 16.0f;
    
    return shadowFactor;
}

// https://blog.demofox.org/2022/01/01/interleaved-gradient-noise-a-different-kind-of-low-discrepancy-sequence/
float InterleavedGradientNoise(float2 pixPos)
{
    return frac(52.9829189f * frac(dot(pixPos, float2(0.06711056f, 0.00583715f))));
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
    
    float3 total = float3(0.0f, 0.0f, 0.0f);
    
    uint index;
    float alpha;
    CalcCSMIndex(input.distView, index, alpha);
    
    // Check CSM boundaries
    //if (index == 0)
    //{
    //    return lerp(float4(1.0f, 0.0f, 0.0f, 1.0f), float4(0.0f, 1.0f, 0.0f, 1.0f), alpha);
    //}
    //else if (index == 1)
    //{
    //    return lerp(float4(0.0f, 1.0f, 0.0f, 1.0f), float4(0.0f, 0.0f, 1.0f, 1.0f), alpha);
    //}
    //else if (index == 2)
    //{
    //    return lerp(float4(0.0f, 0.0f, 1.0f, 1.0f), float4(1.0f, 0.0f, 1.0f, 1.0f), alpha);
    //}
    //else if (index == 3)
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
    
    [loop]
    for (uint i = 0; i < 2; ++i)
    {
        LightConstants light = LightConstantBuffers[i];
        
        float shadowFactor;
        
        // Directional
        if (light.type == 0)
        {
            // First cascade
            {
                float4 lightScreen = mul(float4(input.posWorld, 1.0f), light.viewProjection[index]);
                lightScreen.xyz /= lightScreen.w;
                float2 lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
        
                shadowFactor = PCF(i, index, filterSize, lightTexCoord, lightScreen.z, rot);
            }
        
            // Second cascade. Only apply when overlapping can occur.
            if (index < MAX_CASCADES - 1)
            {
                float4 lightScreen = mul(float4(input.posWorld, 1.0f), light.viewProjection[index + 1]);
                lightScreen.xyz /= lightScreen.w;
                float2 lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
        
                float t = PCF(i, index + 1, filterSize, lightTexCoord, lightScreen.z, rot);
                shadowFactor = lerp(shadowFactor, t, alpha);
            }
        
            // Shading in world space
            //float3 toLightWorld = normalize(light.lightPos - input.posWorld);
            float3 toLightWorld = normalize(-light.lightDir); // For directional light, exceptionally.
            float3 halfWay = normalize(toLightWorld + toCameraWorld);
    
            float3 texColor = g_albedo.Sample(g_sampler, texCoord).rgb;
            float3 normal = g_normalMap.Sample(g_sampler, texCoord).rgb * 2.0f - 1.0f;
            float3 normalWorld = mul(normal, TBN);
    
            // Ambient
            float3 ambient = materialAmbient * texColor;
    
            // Diffuse
            float nDotL = max(dot(normalWorld, toLightWorld), 0.0f);
            float3 diffuse = texColor * nDotL * light.lightColor * light.lightIntensity;
    
            // Specular
            float nDotH = max(dot(normalWorld, halfWay), 0.0f);
            float3 specular = pow(nDotH, shininess) * materialSpecular * light.lightColor * light.lightIntensity;
        
            total += ambient + (diffuse + specular) * shadowFactor;
        }
        // Point
        else if (light.type == 1)
        {
            float dist = distance(input.posWorld ,light.lightPos);
            if (dist <= light.range)
                total = float3(1.0f, 1.0f, 1.0f);
        }
        // Spot
        else if (light.type == 2)
        {
            
        }
    }
    
    return float4(total, 1.0f);
}