struct InstanceData
{
    float4x4 world;
    float4x4 inverseTranspose;
    uint materialIndex;
};

StructuredBuffer<InstanceData> g_instanceData : register(t0, space0);

struct VSInput
{
    float3 pos : POSITION;
    float2 texCoord : TEXCOORD;
    float4 tangent : TANGENT;
    float3 normal : NORMAL;
    uint instanceIndex : INSTANCE_INDEX;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float3 posWorld : POSITION;
#ifndef DEPTH_ONLY
    float2 texCoord : TEXCOORD0;
    float3 tangentWorld : TANGENT;
    float3 normalWorld : NORMAL;
    nointerpolation float tangentW : TEXCOORD1; // Do not interpolate w component of tangent vector.
    nointerpolation uint materialIndex : INSTANCE_MATERIAL_INDEX;
#endif  // DEPTH_ONLY
};

cbuffer CameraConstantBuffer : register(b0, space0)
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
    
    InstanceData instanceData = g_instanceData[input.instanceIndex];
    
    output.posWorld = mul(float4(input.pos, 1.0f), instanceData.world).xyz;
    output.pos = mul(float4(output.posWorld, 1.0f), mul(view, projection));
#ifndef DEPTH_ONLY
    output.texCoord = input.texCoord;
    output.tangentWorld = normalize(mul(float4(input.tangent.xyz, 0.0f), instanceData.inverseTranspose).xyz);
    output.normalWorld = normalize(mul(float4(input.normal, 0.0f), instanceData.inverseTranspose).xyz);
    output.tangentW = input.tangent.w;
    output.materialIndex = instanceData.materialIndex;
#endif  // DEPTH_ONLY
    
    return output;
}
