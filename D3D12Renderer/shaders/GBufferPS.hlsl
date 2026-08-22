#include "PSInput.hlsli"
#include "SurfaceData.hlsli"
#include "CameraConstants.hlsli"

struct GBufferOutput
{
    float4 albedo : SV_Target0;
    float4 normal : SV_Target1; // pack with shininess
    float4 materialAmbient : SV_Target2;
    float4 materialSpecular : SV_Target3;
};

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
