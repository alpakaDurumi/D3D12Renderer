Texture2D g_albedo : register(t0, space0);
Texture2D g_normalMap : register(t1, space0);
Texture2D g_heightMap : register(t2, space0);

Texture2D<float> g_shadowMaps[] : register(t0, space1);

SamplerState g_sampler : register(s0);
SamplerComparisonState g_samplerComparison : register(s1);

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1; // Do not interpolate w component of tangent vector.
};

cbuffer MeshConstantBuffer : register(b0, space0)
{
    float4x4 world;
    float4x4 inverseTranspose;
    float textureTileScale;
};

cbuffer MaterialConstantBuffer : register(b1, space0)
{
    float3 materialAmbient;
    float3 materialSpecular;
    float shininess;
}

cbuffer CameraConstantBuffer : register(b2, space0)
{
    float3 cameraPos;
    float4x4 viewProjection;
}

struct LightConstants
{
    float3 lightPos;
    float3 lightDir;
    float3 lightColor;
    float lightIntensity;
    float4x4 viewProjection;
};
ConstantBuffer<LightConstants> LightConstantBuffers[] : register(b0, space1);

// Parallax Occlusion Mapping
float2 ParallaxMapping(float2 texCoord, float3 toCamera)
{
    const float heightScale = 0.02f;
    
    // Set numLayers based on view direction
    const float minLayers = 8.0f;
    const float maxLayers = 32.0f;
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

    // Clip if texCoord exceeds boundary
    if (texCoord.x < 0.0 || texCoord.x > 1.0 * textureTileScale || texCoord.y < 0.0 || texCoord.y > 1.0 * textureTileScale)
    {
        clip(-1);
    }
    
    // Set up TBN matrix
    float3 B = input.tangentW * cross(input.normalWorld, input.tangentWorld);
    float3x3 TBN = float3x3(input.tangentWorld, B, input.normalWorld);
    
    
    float3 total = float3(0.0f, 0.0f, 0.0f);
    
    [loop]
    for (uint i = 0; i < 1; ++i)
    {
        LightConstants light = LightConstantBuffers[i];
        
        float4 lightScreen = mul(float4(input.posWorld, 1.0f), light.viewProjection);
        lightScreen.xyz /= lightScreen.w;
        
        float2 lightTexCoord = float2((lightScreen.x + 1.0f) * 0.5f, 1.0f - (lightScreen.y + 1.0f) * 0.5f);
        float shadowFactor = g_shadowMaps[i].SampleCmpLevelZero(g_samplerComparison, lightTexCoord, lightScreen.z);
        
        // Shading in world space
        float3 toLightWorld = normalize(light.lightPos - input.posWorld);
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
    
    return float4(total, 1.0f);
}