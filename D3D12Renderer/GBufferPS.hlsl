#include "PSInput.hlsli"
#include "SurfaceData.hlsli"

struct GBufferOutput
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1; // pack with shininess
    float4 materialAmbient : SV_Target2;
    float4 materialSpecular : SV_Target3;
};

cbuffer CameraConstantBuffer : register(b0, space0)
{
    float3 cameraPos;
    float4x4 view;
    float4x4 projection;
    float4x4 invView;
    float4x4 invProj;
}

GBufferOutput main(MeshPSInput input)
{
    SurfaceData data = CalcSurfaceData(input, cameraPos);
    
    GBufferOutput output;
    
    // Pack
    output.albedo = data.albedo;
    output.normal = float4(data.normalWorld, data.shininess);
    output.materialAmbient = data.ambient;
    output.materialSpecular = data.specular;
    
    return output;
}
