struct PSInput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD;
};

PSInput main(float3 pos : POSITION, float2 uv : TEXCOORD)
{
	PSInput output;
	
	output.position = float4(pos, 1.0f);
	output.uv = uv;
	
	return output;
}