Texture2D g_texture : register(t0);
Texture2D g_normalMap : register(t1);
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

float4 main(PSInput input) : SV_TARGET
{
	// Shading in tangent space
	
    float3 toLight = normalize(input.lightPosTangent - input.posTangent);
    float3 toCamera = normalize(input.cameraPosTangent - input.posTangent);
	float3 halfWay = normalize(toLight + toCamera);
	
    float3 texColor = g_texture.Sample(g_sampler, input.texCoord * textureTileScale).rgb;
    float3 normalTangent = g_normalMap.Sample(g_sampler, input.texCoord * textureTileScale).rgb * 2.0f - 1.0f;
	
	// Ambient
	float3 ambient = materialAmbient * texColor;
	
	// Diffuse
    float nDotL = max(dot(normalTangent, toLight), 0.0f);
	float3 diffuse = texColor * nDotL * lightColor * lightIntensity;
	
	// Specular
    float nDotH = max(dot(normalTangent, halfWay), 0.0f);
    float3 specular = pow(nDotH, shininess) * materialSpecular * lightColor * lightIntensity;
	
	return float4(ambient + diffuse + specular, 1.0f);
}