#include "PSInput.hlsli"

FullScreenPSInput main(uint vid : SV_VertexID)
{
    float2 ndc = float2(
           (vid == 2) ? 3.0f : -1.0f,
           (vid == 1) ? 3.0f : -1.0f
       );

    FullScreenPSInput output;
    output.pos = float4(ndc, 0.0f, 1.0f);
    output.texCoord = float2(ndc.x, -ndc.y) * 0.5f + 0.5f;
    return output;
}
