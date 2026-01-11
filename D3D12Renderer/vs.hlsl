struct VSInput
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD;
    float4 tangent : TANGENT;
    float3 normal : NORMAL;
#ifdef INSTANCED
    float4x4 instanceWorld : INSTANCE_WORLD;
    float4x4 instanceInverseTranspose : INSTANCE_INVTRANSPOSE;
#endif
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1;     // Do not interpolate w component of tangent vector.
    float distView : TEXCOORD2;                     // Distance in view space for determining CSM index.
};

cbuffer MeshConstantBuffer : register(b0)
{
    float4x4 world;
    float4x4 inverseTranspose;
    float textureTileScale;
};

cbuffer CameraConstantBuffer : register(b2)
{
    float3 cameraPos;
    float4x4 view;
    float4x4 projection;
}

PSInput main(VSInput input)
{
    PSInput output;
    
    // Calculate position in world space
#ifdef INSTANCED
    output.posWorld = mul(float4(input.pos, 1.0f), mul(world, input.instanceWorld)).xyz;
#else
    output.posWorld = mul(float4(input.pos, 1.0f), world).xyz;
#endif
    
    float3 posView = mul(float4(output.posWorld, 1.0f), view).xyz;
    output.distView = posView.z;
    output.pos = mul(float4(posView, 1.0f), projection);
    output.texCoord = input.texCoord;
    
#ifndef DEPTH_ONLY
#ifdef INSTANCED
    output.tangentWorld = normalize(mul(float4(input.tangent.xyz, 0.0f), mul(input.instanceInverseTranspose, inverseTranspose)).xyz);
    output.normalWorld = normalize(mul(float4(input.normal, 0.0f), mul(input.instanceInverseTranspose, inverseTranspose)).xyz);
#else   // INSTANCED
    output.tangentWorld = normalize(mul(float4(input.tangent.xyz, 0.0f), inverseTranspose).xyz);
    output.normalWorld = normalize(mul(float4(input.normal, 0.0f), inverseTranspose).xyz);
#endif  // INSTANCED
    output.tangentW = input.tangent.w;
#endif  // SHADOW
    
    return output;
}