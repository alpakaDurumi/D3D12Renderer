Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

struct PSInput
{
	float4 pos : SV_POSITION;
	float3 posWorld : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

cbuffer MaterialConstantBuffer : register(b1)
{
	float3 materialAmbient;
    float textureTileScale;
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

float4 main(PSInput input) : SV_TARGET
{
	float3 toLight = normalize(lightPos - input.posWorld);
	float3 toCamera = normalize(cameraPos - input.posWorld);
	float3 halfWay = normalize(toLight + toCamera);
	
    float3 texColor = g_texture.Sample(g_sampler, input.texCoord * textureTileScale).rgb;
	
	// Ambient
	float3 ambient = materialAmbient * texColor;
	
	// Diffuse
	float nDotL = max(dot(input.normal, toLight), 0.0f);
	float3 diffuse = texColor * nDotL * lightColor * lightIntensity;
	
	// Specular
	float nDotH = max(dot(input.normal, halfWay), 0.0f);
    float3 specular = pow(nDotH, shininess) * materialSpecular * lightColor * lightIntensity;
	
	return float4(ambient + diffuse + specular, 1.0f);
}