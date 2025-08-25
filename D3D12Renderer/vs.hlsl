struct PSInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

PSInput main(float3 pos : POSITION, float4 color : COLOR)
{
	PSInput output;
	
	output.position = float4(pos, 1.0f);
	output.color = color;
	
	return output;
}