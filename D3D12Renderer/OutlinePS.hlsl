Texture2D<float> g_selectionMask : register(t0, space6);
Texture2D<float> g_horizontalDilatedMask : register(t1, space6);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

cbuffer OutlineConstantBuffer : register(b4, space0)
{
    int thickness;
}

static const float4 OutlineColor = float4(1.0f, 0.5f, 0.0f, 1.0f);      // Orange

float4 main(PSInput input) : SV_TARGET
{
    int2 coord = int2(input.pos.xy);
    
    if (g_selectionMask.Load(int3(coord, 0)).r > 0.5f)
        discard;
    
    for (int dy = -thickness; dy <= thickness; ++dy)
    {
        if (g_horizontalDilatedMask.Load(int3(coord + int2(0, dy), 0)).r > 0.5f)
            return OutlineColor;
    }
    
    discard;
    
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}