struct PSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
};

float4 main(PSInput input) : SV_TARGET
{
    return 1.0f;
}
