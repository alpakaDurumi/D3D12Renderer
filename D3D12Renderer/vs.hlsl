struct VSInput
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
#ifdef INSTANCED
	float4x4 instanceWorld : INSTANCE_WORLD;
	float4x4 instanceInverseTranspose : INSTANCE_INVTRANSPOSE;
#endif
};

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

cbuffer CameraConstantBuffer : register(b3)
{
    float3 cameraPos;
    float4x4 viewProjection;
}

PSInput main(VSInput input)
{
    PSInput output;
	
#ifdef INSTANCED
	output.posWorld = mul(float4(input.pos, 1.0f), mul(world, input.instanceWorld)).xyz;
#else
    output.posWorld = mul(float4(input.pos, 1.0f), world).xyz;
#endif
    
    output.pos = mul(float4(output.posWorld, 1.0f), viewProjection);
    output.texCoord = input.texCoord;
    
#ifdef INSTANCED
	output.normalWorld = normalize(mul(float4(input.normal, 0.0f), mul(input.instanceInverseTranspose, inverseTranspose)).xyz);
    output.tangentWorld = normalize(mul(float4(input.tangent, 0.0f), mul(input.instanceInverseTranspose, inverseTranspose)).xyz);
#else
    output.normalWorld = normalize(mul(float4(input.normal, 0.0f), inverseTranspose).xyz);
    output.tangentWorld = normalize(mul(float4(input.tangent, 0.0f), inverseTranspose).xyz);
#endif
	
    return output;
}