struct VSInput
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD;
    float4 tangent : TANGENT;
    float3 normal : NORMAL;
    float4x4 instanceWorld : INSTANCE_WORLD;
    float4x4 instanceInverseTranspose : INSTANCE_INVTRANSPOSE;
    uint materialIndex : INSTANCE_MATERIAL_INDEX;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1;     // Do not interpolate w component of tangent vector.
    nointerpolation uint materialIndex : INSTANCE_MATERIAL_INDEX;
};

cbuffer CameraConstantBuffer : register(b0)
{
    float3 cameraPos;
    float4x4 view;
    float4x4 projection;
    float4x4 invView;
    float4x4 invProj;
}

PSInput main(VSInput input)
{
    PSInput output;
    
    output.posWorld = mul(float4(input.pos, 1.0f), input.instanceWorld).xyz;
    output.pos = mul(float4(output.posWorld, 1.0f), mul(view, projection));
    output.texCoord = input.texCoord;
    
#ifndef DEPTH_ONLY
    output.tangentWorld = normalize(mul(float4(input.tangent.xyz, 0.0f), input.instanceInverseTranspose).xyz);
    output.normalWorld = normalize(mul(float4(input.normal, 0.0f), input.instanceInverseTranspose).xyz);
    output.tangentW = input.tangent.w;
    output.materialIndex = input.materialIndex;
#endif  // DEPTH_ONLY
    
    return output;
}