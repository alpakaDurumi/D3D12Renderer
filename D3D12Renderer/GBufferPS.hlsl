#include "PSInput.hlsli"
#include "POM.hlsli"

Texture2D g_textures[] : register(t0, space1);

// Dynamic samplers for ordinary texture sampling
SamplerState g_samplers[] : register(s0, space0);

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

struct MaterialConstants
{
    float3 materialAmbient;
    float3 materialSpecular;
    float shininess;
    uint4 textureIndices;
    uint4 samplerIndices;
    float4 textureTileScales;
};

ConstantBuffer<MaterialConstants> MaterialConstantBuffers[] : register(b0, space1);

GBufferOutput main(MeshPSInput input)
{
    GBufferOutput output;
    
    uint materialIdx = input.materialIndex;
    
    uint4 textureIndices = MaterialConstantBuffers[materialIdx].textureIndices;
    uint albedoIdx = textureIndices[0];
    uint normalMapIdx = textureIndices[1];
    uint heightMapIdx = textureIndices[2];
    
    uint4 samplerIndices = MaterialConstantBuffers[materialIdx].samplerIndices;
    uint albedoSamplerIdx = samplerIndices[0];
    uint normalMapSamplerIdx = samplerIndices[1];
    uint heightMapSamplerIdx = samplerIndices[2];
    
    float4 textureTileScales = MaterialConstantBuffers[materialIdx].textureTileScales;
    float albedoScale = textureTileScales[0];
    float normalMapScale = textureTileScales[1];
    float heightMapScale = textureTileScales[2];
    
    // For POM, use inaccurate inverse-TBN
    float3 iT = normalize(input.tangentWorld);
    float3 iN = normalize(input.normalWorld);
    iT = normalize(iT - dot(iT, iN) * iN); // Gram-Schmidt process
    float3 iB = input.tangentW * cross(iN, iT); // W represents handedness
    float3x3 iiTBN = transpose(float3x3(iT, iB, iN)); // Inverse of orthogonal matrix is same as transpose.
    
    float3 toCameraWorld = normalize(cameraPos - input.posWorld);
    float3 toCameraTangent = normalize(mul(toCameraWorld, iiTBN));
    
    float2 texCoord = ParallaxMapping(input.texCoord * heightMapScale, toCameraTangent, g_textures[heightMapIdx], g_samplers[heightMapSamplerIdx]);
    float2 albedoTexCoord = texCoord * albedoScale / heightMapScale;
    float2 normalMapTexCoord = texCoord * normalMapScale / heightMapScale;
    
    // Set up TBN matrix
    float3 B = input.tangentW * cross(input.normalWorld, input.tangentWorld);
    float3x3 TBN = float3x3(input.tangentWorld, B, input.normalWorld);
    
    // Sample textures
    float3 texColor = g_textures[albedoIdx].Sample(g_samplers[albedoSamplerIdx], albedoTexCoord).rgb;
    float3 normal = g_textures[normalMapIdx].Sample(g_samplers[normalMapSamplerIdx], normalMapTexCoord).rgb * 2.0f - 1.0f;
    float3 normalWorld = normalize(mul(normal, TBN));
    
    output.albedo = float4(texColor, 1.0f);
    output.normal = float4(normalWorld, MaterialConstantBuffers[materialIdx].shininess);
    output.materialAmbient = float4(MaterialConstantBuffers[materialIdx].materialAmbient, 1.0f);
    output.materialSpecular = float4(MaterialConstantBuffers[materialIdx].materialSpecular, 1.0f);
    
    return output;
}
