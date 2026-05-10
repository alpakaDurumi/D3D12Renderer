// Actual tone mapping is not performed yet

Texture2D g_sceneColorBuffer : register(t0, space6);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    int2 coord = int2(input.pos.xy);
    return g_sceneColorBuffer.Load(int3(coord, 0));
}