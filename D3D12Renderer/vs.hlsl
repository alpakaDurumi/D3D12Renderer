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

PSInput main(float3 pos : POSITION, float2 texCoord : TEXCOORD)
{
	PSInput output;
	
	output.position = mul(mul(mul(float4(pos, 1.0f), world), view), projection);
	output.texCoord = texCoord;
	
	return output;
}