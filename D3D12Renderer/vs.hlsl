struct VSInput
{
	float3 pos : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4x4 instanceWorld : INSTANCE_WORLD;
	float4x4 instanceInverseTranspose : INSTANCE_INVTRANSPOSE;
};

struct PSInput
{
	float4 pos : SV_POSITION;
	float3 posWorld : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
};

cbuffer SceneConstantBuffer : register(b2)
{
	float4x4 world;
	float4x4 view;
	float4x4 projection;
	float4x4 inverseTranspose;
};

PSInput main(VSInput input)
{
	PSInput output;
	
	output.posWorld = mul(float4(input.pos, 1.0f), mul(world, input.instanceWorld)).xyz;
	output.pos = mul(mul(float4(output.posWorld, 1.0f), view), projection);
	output.texCoord = input.texCoord;
	output.normal = mul(float4(input.normal, 0.0f), mul(input.instanceInverseTranspose, inverseTranspose)).xyz;
	
	return output;
}