struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

cbuffer SceneConstantBuffer : register(b0)
{
	float4 offset;
	float4 padding[15];
};

PSInput main(float3 pos : POSITION, float2 uv : TEXCOORD)
{
	PSInput output;
	
	output.position = float4(pos, 1.0f) + offset;
	output.uv = uv;
	
	return output;
}