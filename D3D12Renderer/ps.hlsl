Texture2D g_texture : register(t0);
Texture2D g_normalMap : register(t1);
Texture2D g_heightMap : register(t2);

SamplerState g_sampler : register(s0);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
    float3 posTangent : POSITION0;
    float3 lightPosTangent : POSITION1;
    float3 cameraPosTangent : POSITION2;
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

// Parallax Occlusion Mapping
float2 ParallaxMapping(float2 texCoord, float3 toCamera)
{
    const float heightScale = 0.02f;
	
    // Set numLayers based on view direction
    const float minLayers = 8.0f;
    const float maxLayers = 32.0f;
    const float numLayers = lerp(maxLayers, minLayers, max(dot(float3(0.0f, 0.0f, 1.0f), toCamera), 0.0f));
    
    float layerStep = 1.0f / numLayers;
	
    float currentLayerHeight = 0.0f;
	
    // Texture coord offset per layer
    // xy / z = offset / (1.0 * heightScale)
    float2 deltaTexCoord = toCamera.xy / toCamera.z * heightScale / numLayers; 
	
    float2 currentTexCoord = texCoord * textureTileScale;
    float currentHeightMapValue = 1.0f - g_heightMap.Sample(g_sampler, currentTexCoord).r;
    
	[loop]
    while (currentLayerHeight < currentHeightMapValue)
    {
        currentTexCoord -= deltaTexCoord;
        currentHeightMapValue = 1.0f - g_heightMap.Sample(g_sampler, currentTexCoord).r;
        currentLayerHeight += layerStep;
    }
    
    // Interpolate
    float2 prevTexCoord = currentTexCoord + deltaTexCoord;  
    
    float diffAfter = currentLayerHeight - currentHeightMapValue;
    float diffBefore = 1.0f - g_heightMap.Sample(g_sampler, prevTexCoord).r - (currentLayerHeight - layerStep);
    
    float weight = diffBefore / (diffAfter + diffBefore);
    float2 interpolatedTexCoord = lerp(prevTexCoord, currentTexCoord, weight);
    
    return interpolatedTexCoord;
}

float4 main(PSInput input) : SV_TARGET
{
	// Shading in tangent space
	
    float3 toLight = normalize(input.lightPosTangent - input.posTangent);
    float3 toCamera = normalize(input.cameraPosTangent - input.posTangent);
	float3 halfWay = normalize(toLight + toCamera);
	
    float2 texCoord = ParallaxMapping(input.texCoord, toCamera);

    // Clip if texCoord exceeds boundary
    if (texCoord.x < 0.0 || texCoord.x > 1.0 * textureTileScale || texCoord.y < 0.0 || texCoord.y > 1.0 * textureTileScale)
    {
        clip(-1);
    }
	
    float3 texColor = g_texture.Sample(g_sampler, texCoord).rgb;
    float3 normal = g_normalMap.Sample(g_sampler, texCoord).rgb * 2.0f - 1.0f;
	
	// Ambient
	float3 ambient = materialAmbient * texColor;
	
	// Diffuse
    float nDotL = max(dot(normal, toLight), 0.0f);
	float3 diffuse = texColor * nDotL * lightColor * lightIntensity;
	
	// Specular
    float nDotH = max(dot(normal, halfWay), 0.0f);
    float3 specular = pow(nDotH, shininess) * materialSpecular * lightColor * lightIntensity;
	
	return float4(ambient + diffuse + specular, 1.0f);
}