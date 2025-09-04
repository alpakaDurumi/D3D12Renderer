	struct VSInput
{
	float3 pos : POSITION;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float4x4 instanceTransform : INSTANCE_TRANSFORM;
};

struct PSInput
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD;
};

cbuffer SceneConstantBuffer : register(b0)
{
	float4x4 world;
	float4x4 view;
	float4x4 projection;
	float4 padding[4];
};

PSInput main(VSInput input)
{
	PSInput output;
	
	output.position = mul(mul(mul(mul(float4(input.pos, 1.0f), input.instanceTransform), world), view), projection);
	//output.position = mul(mul(mul(float4(input.pos, 1.0f), world), view), projection);
	output.texCoord = input.texCoord;
	
	return output;
}