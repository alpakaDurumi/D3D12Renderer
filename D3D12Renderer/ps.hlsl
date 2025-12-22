Texture2D g_texture : register(t0);
Texture2D g_normalMap : register(t1);
Texture2D g_heightMap : register(t2);

SamplerState g_sampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1; // Do not interpolate w component of tangent vector.
};

cbuffer MeshConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 inverseTranspose;
    float textureTileScale;
};

cbuffer MaterialConstantBuffer : register(b1)
{
	float3 materialAmbient;
	float3 materialSpecular;
	float shininess;
}

cbuffer LightConstantBuffer : register(b2)
{
	float3 lightPos;
	float3 lightDir;
	float3 lightColor;
	float lightIntensity;
}

cbuffer CameraConstantBuffer : register(b3)
{
    float3 cameraPos;
    float4x4 viewProjection;
}

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
    float currentHeightMapValue = 1.0f - g_heightMap.Sample(g_sampler, currentTexCoord).r;
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
        currentHeightMapValue = 1.0f - g_heightMap.Sample(g_sampler, currentTexCoord).r;
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
    
    // Shading in world space
    float3 toLightWorld = normalize(lightPos - input.posWorld);
    float3 halfWay = normalize(toLightWorld + toCameraWorld);
    
    float3 texColor = g_texture.Sample(g_sampler, texCoord).rgb;
    float3 normal = g_normalMap.Sample(g_sampler, texCoord).rgb * 2.0f - 1.0f;
    float3 normalWorld = mul(normal, TBN);
	
	// Ambient
	float3 ambient = materialAmbient * texColor;
	
	// Diffuse
    float nDotL = max(dot(normalWorld, toLightWorld), 0.0f);
	float3 diffuse = texColor * nDotL * lightColor * lightIntensity;
	
	// Specular
    float nDotH = max(dot(normalWorld, halfWay), 0.0f);
    float3 specular = pow(nDotH, shininess) * materialSpecular * lightColor * lightIntensity;
	
	return float4(ambient + diffuse + specular, 1.0f);
}