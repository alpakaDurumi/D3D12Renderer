Texture2D<float> g_selectionMask : register(t0, space6);

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

cbuffer OutlineConstantBuffer : register(b4, space0)
{
    int thickness;
}

float4 main(PSInput input) : SV_TARGET
{
    int2 coord = int2(input.pos.xy);
    for (int dx = -thickness; dx <= thickness; ++dx)
    {
        if (g_selectionMask.Load(int3(coord + int2(dx, 0), 0)).r > 0.5f)
            return 1.0f;
    }
    return 0.0f;
}