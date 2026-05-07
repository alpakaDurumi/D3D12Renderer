Texture2D<uint2> g_stencilBuffer : register(t1, space5);

static const float4 OutlineColor = float4(1.0f, 0.5f, 0.0f, 1.0f);  // Orange

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 texCoord : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
    int2 coord = int2(input.pos.xy);
    
    uint c = g_stencilBuffer.Load(int3(coord, 0)).g;
    uint l = g_stencilBuffer.Load(int3(coord + int2(-1, 0), 0)).g;
    uint r = g_stencilBuffer.Load(int3(coord + int2(1, 0), 0)).g;
    uint u = g_stencilBuffer.Load(int3(coord + int2(0, -1), 0)).g;
    uint d = g_stencilBuffer.Load(int3(coord + int2(0, 1), 0)).g;
    
    bool isEdge = (c != 2) && (l == 2 || r == 2 || u == 2 || d == 2);
    
    if (!isEdge)
        discard;
    
    return OutlineColor;
}