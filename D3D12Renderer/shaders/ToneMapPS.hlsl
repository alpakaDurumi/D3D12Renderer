#include "PSInput.hlsli"

// Actual tone mapping is not performed yet

Texture2D g_sceneColorBuffer : register(t0, space8);

float4 main(FullScreenPSInput input) : SV_TARGET
{
    int2 coord = int2(input.pos.xy);
    return g_sceneColorBuffer.Load(int3(coord, 0));
}
