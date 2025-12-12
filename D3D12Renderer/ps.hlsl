Texture2D g_texture : register(t0);
Texture2D g_normalMap : register(t1);
SamplerState g_sampler : register(s0);

struct PSInput
{
	float4 pos : SV_POSITION;
	float3 posWorld : POSITION;
	float2 texCoord : TEXCOORD;
    float3 normalWorld : NORMAL;
    float3 tangentWorld : TANGENT;
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

float4 main(PSInput input) : SV_TARGET
{
	float3 toLight = normalize(lightPos - input.posWorld);
	float3 toCamera = normalize(cameraPos - input.posWorld);
	float3 halfWay = normalize(toLight + toCamera);
	
    float3 texColor = g_texture.Sample(g_sampler, input.texCoord * textureTileScale).rgb;
	
	// Normal Map
    float3 normalSampled = g_normalMap.Sample(g_sampler, input.texCoord * textureTileScale).rgb;
    normalSampled = 2.0f * normalSampled - 1.0f;
    float3 N = input.normalWorld;
    float3 T = normalize(input.tangentWorld - dot(input.tangentWorld, N) * N);
    float3 B = normalize(cross(N, T));		// Multiply w element of input.tangent if it exists.
    float3 normal = normalize(mul(normalSampled, float3x3(T, B, N)));
	
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